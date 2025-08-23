#include "stdafx.h"
#include "TileMap.h"`
#include "Client.h"
#include "object.h"

void trim(std::string& s) {
	size_t start = s.find_first_not_of(" \t\n\r\f\v");
	s = (start == std::string::npos) ? "" : s.substr(start);

	size_t end = s.find_last_not_of(" \t\n\r\f\v");
	s = (end == std::string::npos) ? "" : s.substr(0, end + 1);
}


void Client::client_initialize()
{
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	if (false == g_krfont.loadFromFile("Jaemin.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	avatar = Character{"Samurai"};
	avatar.move(4, 4);
}

void Client::ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[2])
	{
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		int textureId = packet->visual;

		std::string chName{};
		switch (textureId)
		{
		case 0:
			chName = "Shinobi";
			break;
		case 1:
			chName = "Samurai";
			break;
		case 2:
			chName = "Fighter";
			break;
		default:
			break;
		}

		avatar.id = g_myid;
		avatar.HP = packet->hp;
		avatar.EXP = packet->exp;
		avatar.LV = packet->level;
		avatar.set_HP();
		avatar.set_LV();
		avatar.SetScale(0.5, 0.5);
		avatar.ChangeCharacterAllSprites(chName);
		avatar.move(packet->x, packet->y);
		g_left_x = packet->x - SCREEN_WIDTH / 2;
		g_top_y = packet->y - SCREEN_HEIGHT / 2;
		avatar.show();
	}
	break;

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;
		if (id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
			avatar.LV = my_packet->lv;
			avatar.set_LV();
			avatar.SetScale(0.5, 0.5);
			avatar.show();
		}
		else if (id < MAX_USER) {
			std::string chName{};
			int textureId = my_packet->visual;
			switch (textureId)
			{
			case 0:
				chName = "Shinobi";
				break;
			case 1:
				chName = "Samurai";
				break;
			case 2:
				chName = "Fighter";
				break;
			default:
				break;
			}

			players[id] = Character{chName};
			players[id].id = id;
			players[id].move(my_packet->x, my_packet->y);
			players[id].SetScale(0.5, 0.5);
			players[id].LV = my_packet->lv;
			players[id].set_LV();
			std::string name = my_packet->name;
			trim(name);
			players[id].HP = my_packet->hp;
			players[id].set_HP();
			players[id].set_name(name.c_str());
			players[id].show();
		}
		else {
			std::string chName{};
			int textureId = my_packet->visual;
			switch (textureId)
			{
			case 0:
				chName = "Gotoku";
				break;
			case 1:
				chName = "Onre";
				break;
			case 2:
				chName = "Yurei";
				break;
			default:
				break;
			}
			players[id] = Character{ chName };
			players[id].id = id;
			players[id].HP = my_packet->hp;
			players[id].LV = my_packet->lv;
			players[id].set_LV();
			players[id].set_HP();
			players[id].move(my_packet->x, my_packet->y);
			players[id].SetScale(0.5, 0.5);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			TileMap& tileMap = TileMap::getInstance("map.json", "tmw_desert_spacing.png");
			if (!tileMap.isCollision(my_packet->x, my_packet->y)) {
				avatar.move(my_packet->x, my_packet->y);
				g_left_x = my_packet->x - SCREEN_WIDTH / 2;
				g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
			}
		}
		else {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		break;
	}
	case SC_Ressurection:
	{
		SC_RESURRECTION_PACKET* p = reinterpret_cast<SC_RESURRECTION_PACKET*> (ptr);
		int other_id = p->id;
		if (other_id == g_myid) {
			avatar.HP = p->hp;
			avatar.EXP = p->exp;
			avatar.set_HP();
			avatar.move(p->x, p->y);
			g_left_x = p->x - SCREEN_WIDTH / 2;
			g_top_y = p->y - SCREEN_HEIGHT / 2;
			avatar.show();
		}
		else if (other_id < MAX_USER) {
			players[other_id].HP = p->hp;
			players[other_id].EXP = p->exp;
			players[other_id].move(p->x, p->y);
			players[other_id].set_HP();
			players[other_id].show();
		}
		else {
			//30�� ��.
			players[other_id].HP = p->hp;
			players[other_id].move(p->x, p->y);
			players[other_id].set_HP();
			players[other_id].show();
		}
	}
	break;
	case SC_STAT_CHANGE:
	{
		SC_STAT_CHANGE_PACKET* p = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(ptr);
		int other_id = p->id;

		if (other_id == g_myid) {
			// It's me
			int oldExp = avatar.EXP;
			int oldLv = avatar.LV;
			int oldHp = avatar.HP;

			avatar.EXP = p->exp;
			avatar.LV = p->level;
			avatar.HP = p->hp;
			avatar.set_HP();
			avatar.set_LV();

			if (p->hp <= 0)
				avatar.hide();

			// Log messages for self
			if (oldHp > p->hp) logChange(L"체력 감소: " + std::to_wstring(oldHp - p->hp));
			else if (oldHp < p->hp) logChange(L"체력 회복: " + std::to_wstring(p->hp - oldHp));
			if (oldExp < p->exp) logChange(L"경험치 획득: " + std::to_wstring(p->exp - oldExp));
			else if (oldExp > p->exp) logChange(L"경험치 손실: " + std::to_wstring(oldExp - p->exp));
			if (oldLv < p->level) logChange(L"레벨 업: " + std::to_wstring(p->level));

		}
		else {
			// It's another object
			auto it = players.find(other_id);
			if (it != players.end()) {
				Character& player = it->second;
				player.EXP = p->exp;
				player.LV = p->level;
				player.HP = p->hp;
				player.set_LV();
				player.set_HP();
				if (p->hp <= 0)
					player.hide();
			}
		}
	}
	break;
	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = 
			reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else {
			players.erase(other_id);
		}
		break;
	}
	case SC_CHAT:
	{
		SC_CHAT_PACKET* my_packet = reinterpret_cast<SC_CHAT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.set_chat(my_packet->mess);
		}
		else {
			players[other_id].set_chat(my_packet->mess);
		}

		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}

}

void Client::process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;

	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = MAKEWORD(ptr[0], ptr[1]);
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}

	}
}

void Client::client_main()
{

	TileMap& tileMap = TileMap::getInstance("map.json", "tmw_desert_spacing.png");

	char net_buf[BUF_SIZE];
	size_t received;

	auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error) {
		std::wcout << L"Recv ����!";
		exit(-1);
	}
	if (recv_result == sf::Socket::Disconnected) {
		std::wcout << L"Disconnected\n";
		exit(-1);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	int offsetX = (g_left_x)*TILE_WIDTH;
	int offsetY = (g_top_y)*TILE_WIDTH;


	tileMap.draw(*g_window, offsetX, offsetY);

	avatar.draw();
	for (auto& pl : players) pl.second.draw();
	sf::Text text;
	text.setStyle(1);
	text.setFont(g_font);
	text.setFillColor(sf::Color::White);
	text.setOutlineColor(sf::Color::Black);
	text.setOutlineThickness(3.f);
	char buf[100];
	sprintf_s(buf, "EXP: %d", avatar.EXP);
	text.setString(buf);
	g_window->draw(text);

	sf::Text Logtext;
	
	Logtext.setFont(g_krfont);
	Logtext.setString(logStr);
	Logtext.setScale(0.5f, 0.5f);
	Logtext.setFillColor(sf::Color::White);
	sf::FloatRect textBounds = Logtext.getGlobalBounds();
	Logtext.setPosition(WINDOW_WIDTH/2 - textBounds.width - 10,
						WINDOW_HEIGHT/2 - textBounds.height - 10);

	sf::RectangleShape background(sf::Vector2f(textBounds.width + 20, 
											textBounds.height + 30));

	background.setPosition(Logtext.getPosition().x - 5, Logtext.getPosition().y - 5);
	background.setFillColor(sf::Color(0, 0, 0, 150));

	g_window->draw(background);
	if (m_mess_end_time < chrono::system_clock::now()) {
		Logtext.setString(" ");
	}
		g_window->draw(Logtext);
}
