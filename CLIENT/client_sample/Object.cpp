#include "object.h"

void OBJECT::draw()
{
	if (false == m_showing) return;
	float rx = (m_x - g_left_x) * TILE_WIDTH + 1;
	float ry = (m_y - g_top_y) * TILE_WIDTH + 1;
	m_sprite.setPosition(rx, ry);
	g_window->draw(m_sprite);
	auto size = m_name.getGlobalBounds();
	if (m_mess_end_time < chrono::system_clock::now()) {
		m_name.setPosition(rx + 20 - size.width / 2, ry + 35);
		g_window->draw(m_name);
	}
	else {
		m_chat.setPosition(rx + 32 - size.width / 2, ry - 20);
		g_window->draw(m_chat);
	}
	auto HPsize = m_hpText.getGlobalBounds();
	m_hpText.setPosition(rx + 20 - (HPsize.width / 2), ry - 20);
	g_window->draw(m_hpText);


	auto LVsize = m_LVText.getGlobalBounds();
	m_LVText.setPosition(rx + 20 - (LVsize.width / 2), ry - 35);
	g_window->draw(m_LVText);
}

void OBJECT::set_name(const char str[])
{
	m_name.setFont(g_font);
	m_name.setScale(0.5f, 0.5f);
	m_name.setString(str);
	playerID = str;
	if (id < MAX_USER) m_name.setFillColor(sf::Color(0, 0, 255));
	else m_name.setFillColor(sf::Color(255, 0, 0));
	m_name.setStyle(sf::Text::Bold);
}

void OBJECT::set_chat(const char str[])
{
	m_chat.setFont(g_font);
	m_chat.setString(str);
	m_chat.setFillColor(sf::Color(255, 255, 255));
	m_chat.setStyle(sf::Text::Bold);
	m_chat.setScale(0.4,0.4);
	m_mess_end_time = chrono::system_clock::now() + chrono::seconds(1);
}

void Character::updateSprite()
{
	switch (stat) {
	case STAT::IDLE:
		animation.ChangeSprite(*m_textures[0], sf::Vector2u(6, 1), 0.1f);
		body.setTexture(m_textures[0]);
		break;
	case STAT::WALK:
		animation.ChangeSprite(*m_textures[1], sf::Vector2u(8, 1), 0.1f);
		body.setTexture(m_textures[1]);
		break;
	case STAT::ATTACK_1:
		animation.ChangeSprite(*m_textures[2], sf::Vector2u(6, 1), 0.05f);
		body.setTexture(m_textures[2]);
		break;
	case STAT::ATTACK_2:
		animation.ChangeSprite(*m_textures[3], sf::Vector2u(4, 1), 0.05f);
		body.setTexture(m_textures[3]);
		break;
	}
}