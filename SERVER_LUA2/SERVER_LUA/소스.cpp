#include "stdafx.h"
#include "TiledMap.h"
#include "protocol.h"
#include <vector>
#include <list>

using namespace std;

void ConnectDataBase();

enum COMP_TYPE {
	OP_ACCEPT, OP_RECV, OP_SEND, OP_NPC_MOVE, OP_AI_HELLO, OP_AI_BYE, OP_NPC_RUNAWAY,
	OP_RESURRECTION, OP_HEAL, OP_ReadDB, OP_WriteDB
};

class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	int _ai_target_obj;
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsabuf.len = MAKEWORD(packet[0], packet[1]);
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_send_buf, packet, _wsabuf.len);
	}
};


concurrency::concurrent_queue<OVER_EXP*> over_exp_pool;

void InitializeOverExpPool()
{
	for (int i = 0; i < 200000; ++i) {
		over_exp_pool.push(new OVER_EXP);
	}
}


array<array<list<int>, SECTOR_W_COUNT>, SECTOR_H_COUNT> sectors;
array<array<mutex, SECTOR_W_COUNT>, SECTOR_H_COUNT> sector_lock;

concurrency::concurrent_priority_queue<EVENT> g_timer_queue;
std::random_device rd;
std::uniform_int_distribution<> uid(0, 2);

void add_timer(int obj_id, EVENT_TYPE et, int ms)
{
	EVENT ev;
	ev.obj_id = obj_id;
	ev.et = et;
	ev.wakeup_time = chrono::system_clock::now() + chrono::milliseconds(ms);
	g_timer_queue.push(ev);
}

concurrency::concurrent_queue <QueryTask> QueryQueue;

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
class SESSION {
	OVER_EXP _recv_over;

public:
	mutex _s_lock;
	S_STATE _state;
	atomic_bool	_is_active;
	atomic_bool	_is_healActive;
	bool	_is_npc;
	int _id;
	int hp;
	int FullHP;

	int exp;
	int lv;
	int _visual{};

	mutex	_vll;
	SOCKET _socket;
	short	x, y;
	short	_sector_x, _sector_y;
	short	init_x, init_y;
	char	_name[NAME_SIZE];
	int		_prev_remain;
	unordered_set <int> _view_list;
	mutex	_vl;
	int		last_move_time;
	lua_State* _L;
	mutex	_ll;
	int _target_obj{};
public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		x = y = 0;
		_name[0] = 0;
		_state = ST_FREE;
		_prev_remain = 0;
		_sector_x = -1;
		_sector_y = -1;
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag,
			&_recv_over._over, 0);
	}

	void do_send(void* packet)
	{
		OVER_EXP* sdata;
		if (!over_exp_pool.try_pop(sdata)) {
			sdata = new OVER_EXP(reinterpret_cast<char*>(packet));
		}
		else {
			sdata->_wsabuf.len = MAKEWORD(reinterpret_cast<char*>(packet)[0], reinterpret_cast<char*>(packet)[1]);
			sdata->_wsabuf.buf = sdata->_send_buf;
			ZeroMemory(&sdata->_over, sizeof(sdata->_over));
			sdata->_comp_type = OP_SEND;
			memcpy(sdata->_send_buf, packet, sdata->_wsabuf.len);
		}
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}
	void send_login_info_packet();
	void send_move_packet(int c_id);
	void send_add_player_packet(int c_id);
	void send_resurrect_npc_packet(int c_id);
	void send_resurrect_player_packet(int c_id);
	void send_chat_packet(int c_id, const char* mess);
	void send_remove_player_packet(int c_id);
	void send_login_fail_packet();
	void send_change_state_packet(int c_id);
};

HANDLE h_iocp;
array<SESSION, MAX_USER + MAX_NPC> objects;
SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

void SetSocketOptions(SOCKET sock)
{
	int opt_val = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&opt_val, sizeof(opt_val));
}

bool can_see(int from, int to);
void get_near_objects(int obj_id, unordered_set<int>& near_list);
void WakeUpNPC(int npc_id, int waker);
void add_to_sector(int obj_id, int sector_x, int sector_y);


bool is_pc(int object_id)
{
	return object_id < MAX_USER;
}

bool is_npc(int object_id)
{
	return !is_pc(object_id);
}

void SESSION::send_login_info_packet()
{
	SC_LOGIN_INFO_PACKET p;
	p.id = _id;
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.x = x;
	p.y = y;
	p.visual = _visual;
	p.hp = hp;
	p.max_hp = FullHP;
	p.exp = exp;
	p.level = lv;
	do_send(&p);
}

void handleReadDB(SQLHSTMT hstmt, SQLHDBC hdbc, QueryTask queryTask)
{
	SQLINTEGER  UserPosX, UserPosY, UserEXP, UserHp, UserLV, UserVisual, UserFullHp;
	SQLCHAR playerName[256];
	SQLLEN cbPlayerName = 0;
	SQLLEN cbPosX = 0, cbPosY = 0, cbEXP = 0, cbHP = 0, cbLV = 0, cbVisual = 0, cbFullHp = 0;
	SQLRETURN retcode;

	wstring cmd = L"EXEC GetPlayerINFO ";
	std::string client_name_str(queryTask.client_name);
	cmd += std::wstring(client_name_str.begin(), client_name_str.end());
	int index = queryTask.index;
	retcode = SQLExecDirect(hstmt, (SQLWCHAR*)cmd.c_str(), SQL_NTS);

	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLBindCol(hstmt, 1, SQL_C_CHAR, playerName, sizeof(playerName), &cbPlayerName);
		retcode = SQLBindCol(hstmt, 2, SQL_C_LONG, &UserPosX, 4, &cbPosX);
		retcode = SQLBindCol(hstmt, 3, SQL_C_LONG, &UserPosY, 4, &cbPosY);
		retcode = SQLBindCol(hstmt, 4, SQL_C_LONG, &UserEXP, 4, &cbEXP);
		retcode = SQLBindCol(hstmt, 5, SQL_C_LONG, &UserHp, 4, &cbHP);
		retcode = SQLBindCol(hstmt, 6, SQL_C_LONG, &UserLV, 4, &cbLV);
		retcode = SQLBindCol(hstmt, 7, SQL_C_LONG, &UserVisual, 4, &cbVisual);
		retcode = SQLBindCol(hstmt, 8, SQL_C_LONG, &UserFullHp, 4, &cbFullHp);

		retcode = SQLFetch(hstmt);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			char* source = reinterpret_cast<char*>(playerName);
			size_t numToCopy = min(strlen(source), sizeof(objects[index]._name) - 1);
			std::copy_n(source, numToCopy, objects[index]._name);
			objects[index]._name[numToCopy] = '\0';

			objects[index].x = UserPosX;
			objects[index].y = UserPosY;
			objects[index].init_x = objects[index].x;
			objects[index].init_y = objects[index].y;
			objects[index].exp = UserEXP;
			objects[index].hp = UserHp;
			objects[index].FullHP = UserFullHp;
			objects[index].lv = UserLV;
			objects[index]._id = index;
			objects[index]._visual = UserVisual;
		}
		else if (retcode == SQL_NO_DATA) {
			
			strcpy_s(objects[index]._name, queryTask.client_name);
			objects[index].x = rand() % W_WIDTH;
			objects[index].y = rand() % W_HEIGHT;
			objects[index].init_x = objects[index].x;
			objects[index].init_y = objects[index].y;
			objects[index].hp = 100;
			objects[index].FullHP = 100;
			objects[index].lv = 1;
			objects[index].exp = 0;
			objects[index]._visual = rand() % 3;
			objects[index]._id = index;
		}

		{
			lock_guard<mutex> ll{ objects[index]._s_lock };
			objects[index]._state = ST_INGAME;
		}
		add_to_sector(index, objects[index].x / SECTOR_SIZE, objects[index].y / SECTOR_SIZE);
		objects[index].send_login_info_packet();

		unordered_set<int> near_list;
		get_near_objects(index, near_list);

		for (auto& pl_id : near_list) {
			if (objects[pl_id]._state == ST_INGAME) {
				objects[index].send_add_player_packet(pl_id);
				if (is_pc(pl_id)) {
					objects[pl_id].send_add_player_packet(index);
				}
				else {
					WakeUpNPC(pl_id, index);
				}
			}
		}
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
}


void ConnectDataBase()
{
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;

	setlocale(LC_ALL, "korean");
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
				retcode = SQLConnect(hdbc, (SQLWCHAR*)L"server_2019180024", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					cout << "Connected to DB." << endl;
					while (true) {
						QueryTask queryTask;
						if (QueryQueue.try_pop(queryTask)) {
							retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
							if (queryTask.type == OP_ReadDB) {
								handleReadDB(hstmt, hdbc, queryTask);
							}
							else if (queryTask.type == OP_WriteDB) {
								wstring cmd = L"EXEC SavePlayerPos ";
								int index = queryTask.index;
								std::string client_name(queryTask.client_name);
								cmd += std::wstring(client_name.begin(), client_name.end());
								cmd += L",";
								cmd += to_wstring(objects[index].x);
								cmd += L",";
								cmd += to_wstring(objects[index].y);
								cmd += L",";
								cmd += to_wstring(objects[index].hp);
								cmd += L",";
								cmd += to_wstring(objects[index].lv);
								cmd += L",";
								cmd += to_wstring(objects[index].exp);
								SQLExecDirect(hstmt, (SQLWCHAR*)cmd.c_str(), SQL_NTS);
							}
							SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
						}
						else {
							this_thread::sleep_for(1ms);
						}
					}
					SQLDisconnect(hdbc);
				}
				SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}
}


bool can_see(int from, int to)
{
	if (!(to > -1 && from > -1 && from < objects.size() && to < objects.size()))
		return false;
	if (abs(objects[from].x - objects[to].x) > VIEW_RANGE) return false;
	return abs(objects[from].y - objects[to].y) <= VIEW_RANGE;
}
bool can_move(int x, int y)
{
	TiledMap& tilemap = TiledMap::getInstance();
	return !(tilemap.isCollision(x, y));
}
bool can_Attack(int from, int to)
{
	if (!(to > -1 && from > -1 && from < objects.size() && to < objects.size()))
		return false;
	if (abs(objects[from].x - objects[to].x) > ATTACK_RANGE) return false;
	return abs(objects[from].y - objects[to].y) <= ATTACK_RANGE;
}


void add_to_sector(int obj_id, int sector_x, int sector_y) {
	sector_lock[sector_y][sector_x].lock();
	sectors[sector_y][sector_x].push_back(obj_id);
	sector_lock[sector_y][sector_x].unlock();
	objects[obj_id]._sector_x = sector_x;
	objects[obj_id]._sector_y = sector_y;
}

void remove_from_sector(int obj_id, int sector_x, int sector_y) {
	sector_lock[sector_y][sector_x].lock();
	sectors[sector_y][sector_x].remove(obj_id);
	sector_lock[sector_y][sector_x].unlock();
}

void get_near_objects(int obj_id, unordered_set<int>& near_list)
{
	int my_sector_x = objects[obj_id]._sector_x;
	int my_sector_y = objects[obj_id]._sector_y;

	for (int i = max(0, my_sector_y - 1); i <= min(SECTOR_H_COUNT - 1, my_sector_y + 1); ++i) {
		for (int j = max(0, my_sector_x - 1); j <= min(SECTOR_W_COUNT - 1, my_sector_x + 1); ++j) {
			lock_guard<mutex> lock(sector_lock[i][j]);
			for (int target_id : sectors[i][j]) {
				if (target_id == obj_id) continue;
				if (objects[target_id]._state != ST_INGAME) continue;
				if (can_see(obj_id, target_id)) {
					near_list.insert(target_id);
				}
			}
		}
	}
}

void MoveDoneNPC(int npc_id, int waker)
{
	OVER_EXP* exover;
	if (!over_exp_pool.try_pop(exover)) exover = new OVER_EXP;
	exover->_comp_type = OP_AI_BYE;
	exover->_ai_target_obj = waker;
	PostQueuedCompletionStatus(h_iocp, 1, npc_id, &exover->_over);
}


void SESSION::send_move_packet(int c_id)
{
	SC_MOVE_OBJECT_PACKET p;
	p.id = c_id;
	p.size = sizeof(SC_MOVE_OBJECT_PACKET);
	p.type = SC_MOVE_OBJECT;
	p.x = objects[c_id].x;
	p.y = objects[c_id].y;
	p.move_time = objects[c_id].last_move_time;
	do_send(&p);
}

void SESSION::send_add_player_packet(int c_id)
{
	SC_ADD_OBJECT_PACKET add_packet;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, objects[c_id]._name);
	add_packet.size = sizeof(add_packet);
	add_packet.type = SC_ADD_OBJECT;
	add_packet.x = objects[c_id].x;
	add_packet.y = objects[c_id].y;
	add_packet.lv = objects[c_id].lv;
	add_packet.visual = objects[c_id]._visual;
	add_packet.hp = objects[c_id].hp;
	_vl.lock();
	_view_list.insert(c_id);
	_vl.unlock();
	do_send(&add_packet);
}

void SESSION::send_resurrect_npc_packet(int c_id)
{
	SC_RESURRECTION_PACKET resurrection_packet;
	resurrection_packet.size = sizeof(resurrection_packet);
	resurrection_packet.type = SC_Ressurection;
	resurrection_packet.exp = objects[c_id].exp;
	resurrection_packet.hp = objects[c_id].FullHP;
	resurrection_packet.id = c_id;
	resurrection_packet.x = objects[c_id].x;
	resurrection_packet.y = objects[c_id].y;
	do_send(&resurrection_packet);
}
void SESSION::send_resurrect_player_packet(int c_id)
{
	SC_RESURRECTION_PACKET resurrection_packet;
	resurrection_packet.size = sizeof(resurrection_packet);
	resurrection_packet.type = SC_Ressurection;
	resurrection_packet.exp = objects[c_id].exp;
	resurrection_packet.hp = objects[c_id].FullHP;
	resurrection_packet.id = c_id;
	resurrection_packet.x = objects[c_id].init_x;
	resurrection_packet.y = objects[c_id].init_y;
	do_send(&resurrection_packet);
}

void SESSION::send_chat_packet(int p_id, const char* mess)
{
	SC_CHAT_PACKET packet;
	packet.id = p_id;
	packet.size = sizeof(packet);
	packet.type = SC_CHAT;
	strcpy_s(packet.mess, mess);
	do_send(&packet);
}

void SESSION::send_change_state_packet(int c_id)
{
	SC_STAT_CHANGE_PACKET p;
	p.size = sizeof(SC_STAT_CHANGE_PACKET);
	p.type = SC_STAT_CHANGE;
	p.id = c_id;
	p.max_hp = objects[c_id].FullHP;
	p.hp = objects[c_id].hp;
	p.exp = objects[c_id].exp;
	p.level = objects[c_id].lv;
	do_send(&p);
}

void SESSION::send_remove_player_packet(int c_id)
{
	_vl.lock();
	if (_view_list.count(c_id))
		_view_list.erase(c_id);
	else {
		_vl.unlock();
		return;
	}
	_vl.unlock();
	SC_REMOVE_OBJECT_PACKET p;
	p.id = c_id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	do_send(&p);
}

void SESSION::send_login_fail_packet()
{
	SC_LOGIN_FAIL_PACKET p;
	p.size = sizeof(SC_LOGIN_FAIL_PACKET);
	p.type = SC_LOGIN_FAIL;
	do_send(&p);
}

int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ objects[i]._s_lock };
		if (objects[i]._state == ST_FREE)
			return i;
	}
	return -1;
}

void WakeUpNPC(int npc_id, int waker)
{
	OVER_EXP* exover;
	if (!over_exp_pool.try_pop(exover)) exover = new OVER_EXP;
	exover->_comp_type = OP_AI_HELLO;
	exover->_ai_target_obj = waker;
	PostQueuedCompletionStatus(h_iocp, 1, npc_id, &exover->_over);

	if (objects[npc_id]._is_active) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&objects[npc_id]._is_active, &old_state, true))
		return;
	add_timer(npc_id, EV_RANDOM_MOVE, 0);
}

void process_packet(int c_id, char* packet)
{
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		QueryTask qt;
		size_t copyLen = min(strlen(p->name), sizeof(qt.client_name) - 1);
		std::copy(p->name, p->name + copyLen, qt.client_name);
		qt.client_name[copyLen] = '\0';
		qt.index = c_id;
		qt.type = OP_ReadDB;
		QueryQueue.push(qt);
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		objects[c_id].last_move_time = p->move_time;
		short old_x = objects[c_id].x;
		short old_y = objects[c_id].y;
		short x = old_x;
		short y = old_y;

		switch (p->direction) {
		case 0: if (y > 0) y--; break;
		case 1: if (y < W_HEIGHT - 1) y++; break;
		case 2: if (x > 0) x--; break;
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		if (can_move(x, y)) {
			objects[c_id].x = x;
			objects[c_id].y = y;

			int old_sector_x = old_x / SECTOR_SIZE;
			int old_sector_y = old_y / SECTOR_SIZE;
			int new_sector_x = x / SECTOR_SIZE;
			int new_sector_y = y / SECTOR_SIZE;

			if (old_sector_x != new_sector_x || old_sector_y != new_sector_y) {
				remove_from_sector(c_id, old_sector_x, old_sector_y);
				add_to_sector(c_id, new_sector_x, new_sector_y);
			}
		}

		unordered_set<int> near_list;
		get_near_objects(c_id, near_list);
			
		for (int target_id : near_list) {
			if (is_npc(target_id)) {
				if (objects[c_id].x == objects[target_id].x && objects[c_id].y == objects[target_id].y) {

					int& player_hp = objects[c_id].hp;

					if (player_hp > 0) {
						player_hp -= 10; // Damage

						if (player_hp <= 0) {
							player_hp = 0;
							add_timer(c_id, EV_RESURRECTION, 5000);
						}

						unordered_set<int> notify_list;
						get_near_objects(c_id, notify_list);
						notify_list.insert(c_id);
						
						for (int id_to_notify : notify_list) {
							if (is_pc(id_to_notify)) {
								objects[id_to_notify].send_change_state_packet(c_id);
							}
						}
					}
					break; 
				}
			}
		}

		objects[c_id].send_move_packet(c_id);

		objects[c_id]._vl.lock();
		unordered_set<int> old_vlist = objects[c_id]._view_list;
		objects[c_id]._vl.unlock();

		for (auto& pl_id : near_list) {
			if (old_vlist.find(pl_id) == old_vlist.end()) {
				objects[c_id].send_add_player_packet(pl_id);
				if (is_pc(pl_id)) {
					objects[pl_id].send_add_player_packet(c_id);
				}
				else {
					WakeUpNPC(pl_id, c_id);
				}
			}
			else {
				if (is_pc(pl_id)) {
					objects[pl_id].send_move_packet(c_id);
				}
				else {
					WakeUpNPC(pl_id, c_id);
				}
			}
		}

		for (auto& old_pl_id : old_vlist) {
			if (near_list.find(old_pl_id) == near_list.end()) {
				objects[c_id].send_remove_player_packet(old_pl_id);
				if (is_pc(old_pl_id)) {
					objects[old_pl_id].send_remove_player_packet(c_id);
				}
			}
		}
	}
		break;
	case CS_ATTACK:
	{
		unordered_set<int> near_list;
		get_near_objects(c_id, near_list);

		for (auto& target_id : near_list) {
			if (can_Attack(c_id, target_id)) {
				
				int& target_hp = objects[target_id].hp;
				target_hp -= 20;
				if (target_hp <= 0) {
					target_hp = 0;
					
					if (is_npc(target_id)) {
						objects[c_id].exp += objects[target_id].lv * 10;
						while (objects[c_id].exp >= objects[c_id].lv * 100) {
							objects[c_id].exp -= objects[c_id].lv * 100;
							objects[c_id].lv++;
							objects[c_id].FullHP += 10;
							objects[c_id].hp = objects[c_id].FullHP;
						}
						add_timer(target_id, EV_RESURRECTION, 5000); 
					}
					else {
						// Player kill
						add_timer(target_id, EV_RESURRECTION, 5000);
					}
				}

				unordered_set<int> notify_list;
				get_near_objects(target_id, notify_list);
				notify_list.insert(target_id);
				if (is_pc(c_id)) notify_list.insert(c_id);

				for (auto& p_id : notify_list) {
					if (is_pc(p_id)) {
						objects[p_id].send_change_state_packet(target_id);
						if (is_pc(c_id))
							objects[p_id].send_change_state_packet(c_id);
					}
				}
			}
		}
	}
	break;
	}
}

void disconnect(int c_id)
{
	if (objects[c_id]._sector_x != -1) {
		remove_from_sector(c_id, objects[c_id]._sector_x, objects[c_id]._sector_y);
	}

	objects[c_id]._vl.lock();
	unordered_set <int> vl = objects[c_id]._view_list;
	objects[c_id]._vl.unlock();
	
	for (auto& p_id : vl) {
		if (is_npc(p_id)) continue;
		
		auto& pl = objects[p_id];
		{
			lock_guard<mutex> ll(pl._s_lock);
			if (ST_INGAME != pl._state) continue;
		}
		
		if (pl._id == c_id) continue;
		pl.send_remove_player_packet(c_id);
	}
	if (ST_INGAME == objects[c_id]._state) {
		if (false == objects[c_id]._is_npc) {
			
			QueryTask qt;
			size_t copyLen = min(strlen(objects[c_id]._name), sizeof(qt.client_name) - 1);
			std::copy(objects[c_id]._name, objects[c_id]._name + copyLen, qt.client_name);

			qt.client_name[copyLen] = '\0';
			qt.index = c_id;
			qt.type = OP_WriteDB;
			QueryQueue.push(qt);
		}
	}
	closesocket(objects[c_id]._socket);

	lock_guard<mutex> ll(objects[c_id]._s_lock);
	objects[c_id]._state = ST_FREE;
}

void do_npc_random_move(int npc_id)
{
	SESSION& npc = objects[npc_id];
	unordered_set<int> near_list;
	get_near_objects(npc_id, near_list);

	int x = npc.x;
	int y = npc.y;
	
	switch (rand() % 4) {
	case 0: if (y > 0) y--; break;
	case 1: if (y < W_HEIGHT - 1) y++; break;
	case 2: if (x > 0) x--; break;
	case 3: if (x < W_WIDTH - 1) x++; break;
	}
	if (can_move(x, y)) {
		npc.x = x;
		npc.y = y;
		int old_sector_x = npc._sector_x;
		int old_sector_y = npc._sector_y;
		int new_sector_x = x / SECTOR_SIZE;
		int new_sector_y = y / SECTOR_SIZE;

		if (old_sector_x != new_sector_x || old_sector_y != new_sector_y) {
			remove_from_sector(npc_id, old_sector_x, old_sector_y);
			add_to_sector(npc_id, new_sector_x, new_sector_y);
		}
	}
	
	get_near_objects(npc_id, near_list);

	for (auto pl_id : near_list) {
		if (is_pc(pl_id)) {
			objects[pl_id].send_move_packet(npc_id);
			
			if (npc.x == objects[pl_id].x && npc.y == objects[pl_id].y) {
				int& player_hp = objects[pl_id].hp;
				if (player_hp > 0) {
					player_hp -= 10;
					if (player_hp <= 0) {
						player_hp = 0;
						add_timer(pl_id, EV_RESURRECTION, 5000);
					}

					unordered_set<int> notify_list;
					get_near_objects(pl_id, notify_list);
					notify_list.insert(pl_id);
					for (int id_to_notify : notify_list) {
						if (is_pc(id_to_notify)) {
							objects[id_to_notify].send_change_state_packet(pl_id);
						}
					}
				}
			}
		}
	}
}

void do_npc_resurrection(int npc_id)
{
	SESSION& npc = objects[npc_id];
	npc.hp = npc.FullHP;
	unordered_set<int> near_list;
	get_near_objects(npc_id, near_list);
	for (auto& pl_id : near_list) {
		if (is_pc(pl_id)) {
			objects[pl_id].send_resurrect_npc_packet(npc_id);
		}
	}
}

void do_player_resurrection(int c_id)
{
	objects[c_id].hp = objects[c_id].FullHP;
	objects[c_id].exp = objects[c_id].exp / 2;
	objects[c_id].x = objects[c_id].init_x;
	objects[c_id].y = objects[c_id].init_y;

	int new_sector_x = objects[c_id].x / SECTOR_SIZE;
	int new_sector_y = objects[c_id].y / SECTOR_SIZE;
	
	if (objects[c_id]._sector_x != new_sector_x || objects[c_id]._sector_y != new_sector_y) {
		remove_from_sector(c_id, objects[c_id]._sector_x, objects[c_id]._sector_y);
		add_to_sector(c_id, new_sector_x, new_sector_y);
	}

	unordered_set<int> near_list;
	get_near_objects(c_id, near_list);

	for (auto& pl_id : near_list) {
		if (is_pc(pl_id)) {
			objects[pl_id].send_resurrect_player_packet(c_id);
		}
	}
	objects[c_id].send_resurrect_player_packet(c_id);
}

void player_heal_event(int c_id)
{
	objects[c_id].hp += 10;
	if (objects[c_id].hp > objects[c_id].FullHP) {
		objects[c_id].hp = objects[c_id].FullHP;
	}

	unordered_set<int> near_list;
	get_near_objects(c_id, near_list);

	for (auto& pl_id : near_list) {
		if (is_pc(pl_id)) {
			objects[pl_id].send_change_state_packet(c_id);
		}
	}
	objects[c_id].send_change_state_packet(c_id);

	if (objects[c_id].hp < objects[c_id].FullHP) {
		add_timer(c_id, EV_HEAL, 5000);
	}
}


enum class LogicTaskType {
    PROCESS_PACKET,
    NPC_MOVE,
    NPC_RUNAWAY,
    AI_HELLO,
    AI_BYE,
    RESURRECTION,
    HEAL,
    DISCONNECT
};

struct LogicTask {
    LogicTaskType type;
    int c_id;
    int target_id; 
    char packet[BUF_SIZE];
};

std::queue<LogicTask> logic_queue;
std::mutex logic_queue_lock;
std::condition_variable logic_queue_cv;

void logic_thread()
{
    while (true) {
        LogicTask task;
        {
            std::unique_lock<std::mutex> lock(logic_queue_lock);
            logic_queue_cv.wait(lock, [] { return !logic_queue.empty(); });
            task = logic_queue.front();
            logic_queue.pop();
        }

        switch (task.type) {
        case LogicTaskType::PROCESS_PACKET:
            process_packet(task.c_id, task.packet);
            break;
        case LogicTaskType::NPC_MOVE:
            do_npc_random_move(task.c_id);
            add_timer(task.c_id, EV_RANDOM_MOVE, 2000);
            break;
        case LogicTaskType::NPC_RUNAWAY:
            if (can_see(task.c_id, objects[task.c_id]._target_obj)) {
                MoveDoneNPC(task.c_id, objects[task.c_id]._target_obj);
            }
            break;
        case LogicTaskType::AI_HELLO:
        {
            bool result = false;
            objects[task.c_id]._ll.lock();
            auto L = objects[task.c_id]._L;
        		
            lua_getglobal(L, "event_player_move");
            lua_pushnumber(L, task.target_id);
        		
            lua_pcall(L, 1, 1, 0);
            result = lua_toboolean(L, -1);
            lua_pop(L, 1);
        		
            objects[task.c_id]._ll.unlock();
        		
            if (result) {
                objects[task.c_id]._target_obj = task.target_id;
                add_timer(task.c_id, EV_RUN_AWAY, 3000);
            }
        }
        break;
        case LogicTaskType::AI_BYE:
            break;
        case LogicTaskType::RESURRECTION:
            if (is_npc(task.c_id)) {
                do_npc_resurrection(task.c_id);
            }
            else {
                do_player_resurrection(task.c_id);
            }
            break;
        case LogicTaskType::HEAL:
            player_heal_event(task.c_id);
            break;
        	
        case LogicTaskType::DISCONNECT:
            disconnect(task.c_id);
            break;
        }
    }
}

void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);

		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "\n";
                LogicTask task;
                task.type = LogicTaskType::DISCONNECT;
                task.c_id = static_cast<int>(key);
                {
                    std::lock_guard<std::mutex> lock(logic_queue_lock);
                    logic_queue.push(task);
                }
				
                logic_queue_cv.notify_one();
                if (ex_over->_comp_type == OP_SEND) over_exp_pool.push(ex_over);
                continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			LogicTask task;
			task.type = LogicTaskType::DISCONNECT;
			task.c_id = static_cast<int>(key);
			{
				std::lock_guard<std::mutex> lock(logic_queue_lock);
				logic_queue.push(task);
			}
			logic_queue_cv.notify_one();
			
			if (ex_over->_comp_type == OP_SEND) over_exp_pool.push(ex_over);
			continue;
		}
		bool result = false;
		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				{
					lock_guard<mutex> ll(objects[client_id]._s_lock);
					objects[client_id]._state = ST_ALLOC;
				}
				
				objects[client_id].x = 0;
				objects[client_id].y = 0;
				objects[client_id]._id = client_id;
				objects[client_id]._name[0] = 0;
				objects[client_id]._prev_remain = 0;
				objects[client_id]._socket = g_c_socket;
				
				SetSocketOptions(g_c_socket);

				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				objects[client_id].do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over._over, sizeof(g_a_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + objects[key]._prev_remain;
			char* p = ex_over->_send_buf;
			while (remain_data > 1) {
				unsigned short packet_size = reinterpret_cast<unsigned short*>(p)[0];
				if (remain_data < packet_size) break;

				LogicTask task;
				task.c_id = static_cast<int>(key);
				memcpy(task.packet, p, packet_size);
                task.type = LogicTaskType::PROCESS_PACKET;
				{
					std::lock_guard<std::mutex> lock(logic_queue_lock);
					logic_queue.push(task);
				}
				logic_queue_cv.notify_one();

				p += packet_size;
				remain_data -= packet_size;
			}

			objects[key]._prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}

			objects[key].do_recv();
			break;
		}
		case OP_SEND:
			over_exp_pool.push(ex_over);
			break;
		case OP_NPC_MOVE:
        {
            LogicTask task;
            task.type = LogicTaskType::NPC_MOVE;
            task.c_id = static_cast<int>(key);
            {
                std::lock_guard<std::mutex> lock(logic_queue_lock);
                logic_queue.push(task);
            }
            logic_queue_cv.notify_one();
            over_exp_pool.push(ex_over);
        }
        break;
        case OP_NPC_RUNAWAY: {
            LogicTask task;
            task.type = LogicTaskType::NPC_RUNAWAY;
            task.c_id = static_cast<int>(key);
            {
                std::lock_guard<std::mutex> lock(logic_queue_lock);
                logic_queue.push(task);
            }
            logic_queue_cv.notify_one();
            over_exp_pool.push(ex_over);
        }
                               break;
        case OP_AI_HELLO: {
            LogicTask task;
            task.type = LogicTaskType::AI_HELLO;
            task.c_id = static_cast<int>(key);
            task.target_id = ex_over->_ai_target_obj;
            {
                std::lock_guard<std::mutex> lock(logic_queue_lock);
                logic_queue.push(task);
            }
            logic_queue_cv.notify_one();
            over_exp_pool.push(ex_over);
        }
        break;
        case OP_AI_BYE: {
            LogicTask task;
            task.type = LogicTaskType::AI_BYE;
            task.c_id = static_cast<int>(key);
            {
                std::lock_guard<std::mutex> lock(logic_queue_lock);
                logic_queue.push(task);
            }
            logic_queue_cv.notify_one();
            over_exp_pool.push(ex_over);
        }
        break;
        case OP_RESURRECTION:
        {
            LogicTask task;
            task.type = LogicTaskType::RESURRECTION;
            task.c_id = static_cast<int>(key);
            {
                std::lock_guard<std::mutex> lock(logic_queue_lock);
                logic_queue.push(task);
            }
            logic_queue_cv.notify_one();
            over_exp_pool.push(ex_over);
            break;
        }
        case OP_HEAL:
        {
            LogicTask task;
            task.type = LogicTaskType::HEAL;
            task.c_id = static_cast<int>(key);
            {
                std::lock_guard<std::mutex> lock(logic_queue_lock);
                logic_queue.push(task);
            }
            logic_queue_cv.notify_one();
            over_exp_pool.push(ex_over);
        }
        break;
		}
	}
}

int API_get_x(lua_State* L)
{
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = objects[user_id].x;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L)
{
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = objects[user_id].y;
	lua_pushnumber(L, y);
	return 1;
}

int API_SendMessage(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	lua_pop(L, 4);
	return 0;
}

void InitializeNPC()
{
	cout << "NPC intialize begin.\n";
	for (int i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {
		objects[i]._is_npc = true;
		objects[i].x = rand() % W_WIDTH;
		objects[i].y = rand() % W_HEIGHT;
		objects[i].init_x = objects[i].x;
		objects[i].init_y = objects[i].y;
		objects[i]._id = i;

		int type = uid(rd);
		objects[i]._visual = type;
		std::string monName{};
		if (type == 0) {
			monName = "Gotoku";
			objects[i].hp = 70;
			objects[i].lv = 5;
			objects[i].FullHP = 110;
		}
		else if (type == 1) {
			monName = "Onre";
			objects[i].hp = 100;
			objects[i].lv = 10;
			objects[i].FullHP = 100;
		}
		else if (type == 2) {
			monName = "Yurei";
			objects[i].hp = 150;
			objects[i].lv = 15;
			objects[i].FullHP = 90;
		}
		sprintf_s(objects[i]._name, NAME_SIZE, "%s", monName.c_str());
		objects[i]._state = ST_INGAME;
		add_to_sector(i, objects[i].x / SECTOR_SIZE, objects[i].y / SECTOR_SIZE);
		add_timer(i, EV_RANDOM_MOVE, 2000);

		auto L = objects[i]._L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "npc.lua");
		lua_pcall(L, 0, 0, 0);

		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0);

		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
	}
	cout << "NPC initialize end.\n";
}

void do_timer()
{
	while (true) {
		EVENT ev;
		auto current_time = chrono::system_clock::now();
		if (true == g_timer_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				g_timer_queue.push(ev);
				this_thread::sleep_for(1ms);
				continue;
			}
			OVER_EXP* ov;
			if (!over_exp_pool.try_pop(ov)) ov = new OVER_EXP;
			
			switch (ev.et) {
			case EV_RANDOM_MOVE:
				ov->_comp_type = OP_NPC_MOVE;
				break;
			case EV_RUN_AWAY:
				ov->_comp_type = OP_NPC_RUNAWAY;
				break;
			case EV_RESURRECTION:
				ov->_comp_type = OP_RESURRECTION;
				break;
			case EV_HEAL:
				ov->_comp_type = OP_HEAL;
				break;
			}
			
			PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
			continue;
		}
		this_thread::sleep_for(1ms);
	}
}

int main()
{
	InitializeOverExpPool();
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	InitializeNPC();

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over._comp_type = OP_ACCEPT;
	
	AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);

	thread db_thread{ ConnectDataBase };
	
	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);

	vector <thread> logic_threads;
	for (int i = 0; i < num_threads; ++i)
		logic_threads.emplace_back(logic_thread);

	thread timer_thread{ do_timer };
	
	timer_thread.join();
	for (auto& th : worker_threads)
		th.join();
	for (auto& th : logic_threads)
		th.join();
	
	db_thread.join();
	closesocket(g_s_socket);
	WSACleanup();
}