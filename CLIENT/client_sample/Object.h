#pragma once
#include "stdafx.h"
#include "AnimateSprite.h"

class OBJECT 
{

public:
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		set_name("NONAME");
		m_mess_end_time = chrono::system_clock::now();
	}
	OBJECT() {
		m_showing = false;
	}
public:
	void SetScale(float x, float y) {
		m_sprite.setScale(x, y);
	}
	
	void show()
	{
		m_showing = true; 
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw();
	void set_name(const char str[]);
	void set_chat(const char str[]);

public:
	int id;
	int m_x, m_y;
	char name[NAME_SIZE];
	std::string playerID;

protected:
	bool m_showing;
	sf::Sprite m_sprite;
	sf::Text m_name;
	sf::Text m_chat;
	sf::Text m_hpText;
	sf::Text m_EXPText;
	sf::Text m_LVText;
	chrono::system_clock::time_point m_mess_end_time;
};


enum STAT {
	IDLE,
	WALK,
	RUN,
	ATTACK_1,
	ATTACK_2
};

class Character : public OBJECT
{
public:
	Character(const std::string&  name) {
		m_showing = false;
		set_name("NONAME");
		m_mess_end_time = chrono::system_clock::now();
		chName = name;
		LoadTexture(name);

	}
	Character() { m_showing = false; }
public:
	enum class State {
		Walking,
		Attacking,
		Idle
	} state;

	void Animate(float ElapsedTime) {
		if (isSpriteUpdated) {
			updateSprite();
			isSpriteUpdated = false;
		}
		
		m_Xdir = m_x - pre_pos.x;
		m_Ydir = m_y - pre_pos.y;

		sf::Vector2f movement(0.f, 0.f);
		updateCharacterState(movement, ElapsedTime);

		if (std::abs(m_Xdir) > FLT_EPSILON) {
			faceRight = (m_Xdir > 0);
		}
		else if (std::abs(m_Ydir) > FLT_EPSILON) {
			faceRight = (m_Ydir > 0);
		}

		animation.Update(ElapsedTime, faceRight);
		body.setTextureRect(animation.uvRect);
		if (isWalk) {
			isWalk = false;
			bpositionUpdated = false;
		}
		else if (!isWalk && !bpositionUpdated) {
			pre_pos = sf::Vector2f(m_x, m_y);
			bpositionUpdated = true;
		}
	};

	void updateCharacterState(sf::Vector2f& movement, float deltaTime)
	{

		if (std::abs(m_Xdir) > FLT_EPSILON || std::abs(m_Ydir) > FLT_EPSILON) {
			new_stat = STAT::WALK;
		}
		else {
			new_stat = STAT::IDLE;
		}

		if (stat == STAT::ATTACK_1 || stat == STAT::ATTACK_2) {
			attackAccumTime += deltaTime;
			if (attackAccumTime >= attackTime) {
				attackAccumTime = 0;
				new_stat = STAT::IDLE;
			}
		}

		if (new_stat != stat) {
			isSpriteUpdated = true;
			stat = new_stat;
		}
	}

	void SetTexture(sf::Texture& t) {
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(32, 50, 64, 78));
	}
	void LoadTexture(const std::string& name)
	{
		for (int i{}; i < m_textures.size(); ++i)
			m_textures[i] = new sf::Texture;

		m_textures[0]->loadFromFile("Assets/" + name + "/Idle.png");
		m_textures[1]->loadFromFile("Assets/" + name + "/Walk.png");
		m_textures[2]->loadFromFile("Assets/" + name + "/Attack_1.png");
		m_textures[3]->loadFromFile("Assets/" + name + "/Attack_2.png");
		SetTexture(*m_textures[0]);
	}
	
	void ChangeSprite(int indx) {
		animation.ChangeSprite(*m_textures[indx], sf::Vector2u(6, 1), 0.1f);
	}

	void ChangeCharacterAllSprites(const std::string& name) {
		if (m_textures[0]) {
			m_textures[0]->loadFromFile("Assets/" + name + "/Idle.png");
		}
		if (m_textures[1]) {
			m_textures[1]->loadFromFile("Assets/" + name + "/Walk.png");
		}
		if (m_textures[2]) {
			m_textures[2]->loadFromFile("Assets/" + name + "/Attack_1.png");
		}
		if (m_textures[3]) {
			m_textures[3]->loadFromFile("Assets/" + name + "/Attack_2.png");
		}
		SetTexture(*m_textures[0]);
	}

	void updateSprite();
	void set_HP()
	{
		m_hpText.setFont(g_font);
		m_hpText.setScale(0.5f, 0.5f);
		std::string str = "HP:" + to_string(HP);
		m_hpText.setString(str);
		if (id < MAX_USER) m_hpText.setFillColor(sf::Color(0, 0, 255));
		else m_hpText.setFillColor(sf::Color(255, 0, 0));
		m_hpText.setStyle(sf::Text::Bold);
		m_hpText.setOutlineColor(sf::Color::Yellow);
		m_hpText.setOutlineThickness(3.f);
		if (HP <= 0) {
			hide();
		}
	}
	void set_LV()
	{
		m_LVText.setFont(g_font);
		m_LVText.setScale(0.5f, 0.5f);
		std::string str = "LV:" + to_string(LV);
		m_LVText.setString(str);
		m_LVText.setFillColor(sf::Color(255, 255, 255));
		m_LVText.setStyle(sf::Text::Bold);
		m_LVText.setOutlineColor(sf::Color::Black);
		m_LVText.setOutlineThickness(3.f);
	}
public:
	int HP{};
	int EXP{};
	int LV{};
	std::string chName;
private:
	AnimateSprite animation;
	sf::Texture** textures;
	float speed;
	bool faceRight;
	bool isSpriteUpdated{};
	float attackTime{ 0.4f };
	float attackAccumTime{};
	sf::Vector2f targetPos{};
private:
	std::array<sf::Texture *, 4> m_textures;
	sf::Texture* currentTexture;
	STAT stat = STAT::IDLE;
	STAT new_stat = STAT::IDLE;
	bool isWalk{};
	bool bpositionUpdated{};

	sf::Vector2f pre_pos{};
	sf::RectangleShape body;

	bool m_showing;
	int m_Xdir{ 0 };
	int m_Ydir{ 0 };
};