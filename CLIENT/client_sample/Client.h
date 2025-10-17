#pragma once
#include "object.h"
extern sf::TcpSocket s_socket;
extern Character avatar;

class Client
{
public:
	Client() {
		client_initialize();
	}
	~Client() {
		client_finish();
	}
public:
	void logChange(const std::wstring& message) {
		logStr = message;
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(1);
	}
	void client_initialize();
	void client_finish()
	{
		players.clear();

	}

	void ProcessPacket(char* ptr);
	void process_data(char* net_buf, size_t io_byte);
	void client_main();
	void send_packet(void* packet)
	{
		unsigned char* p = reinterpret_cast<unsigned char*>(packet);
		size_t sent = 0;
		int pSize = MAKEWORD(p[0], p[1]);
		s_socket.send(packet, pSize, sent);
	}
	void Animate(float elapsedTime) {
		for (auto& p : players) {
			p.second.Animate(elapsedTime);
		}
	}
public:
	unordered_map <int, Character> players;
private:
	std::wstring logStr;
	chrono::system_clock::time_point m_mess_end_time;
};