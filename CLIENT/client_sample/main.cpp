#include"stdafx.h"
#include "object.h"
#include "client.h"

int g_left_x;
int g_top_y;
int g_myid;
sf::TcpSocket s_socket;
sf::Font g_font;
sf::Font g_krfont;
Character avatar;
sf::RenderWindow* g_window;

int main()
{
	wcout.imbue(locale("korean"));
	std::string ipAddr{};
	std::cout << "ip: ";
	std::cin >> ipAddr;
	sf::Socket::Status status = s_socket.connect(ipAddr.c_str(), PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}
	std::string strID{};
	std::cout << "id : ";
	std::cin >> strID;
	Client client;
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;
	strcpy_s(p.name, strID.c_str());
	client.send_packet(&p);
	avatar.set_name(p.name);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;
	sf::View view(sf::FloatRect(0, 0, WINDOW_WIDTH/2, WINDOW_HEIGHT/2));
	window.setView(view);
	sf::Clock clock;
	float cooldown = 1.0f; 
	float lastTime = 0.0f;
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyPressed) {
				int direction = -1;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					direction = 2;
					break;
				case sf::Keyboard::Right:
					direction = 3;
					break;
				case sf::Keyboard::Up:
					direction = 0;
					break;
				case sf::Keyboard::Down:
					direction = 1;
					break;
				case sf::Keyboard::A:
				{
					float currentTime = clock.getElapsedTime().asSeconds();
					if (currentTime - lastTime >= cooldown) // 쿨타임 체크
					{
						CS_ATTACK_PACKET p;
						p.size = sizeof(p);
						p.type = CS_ATTACK;
						client.send_packet(&p);
						lastTime = currentTime;
					}
				}
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
				if (-1 != direction) {
					CS_MOVE_PACKET p;
					p.size = sizeof(p);
					p.type = CS_MOVE;
					p.direction = direction;
					client.send_packet(&p);
				}

			}
		}
		
		window.clear();
		client.client_main();
		window.display();
	}
	client.client_finish();

	return 0;
}