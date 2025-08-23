#pragma once
#include "stdafx.h"
class AnimateSprite
{
public:
	AnimateSprite(sf::Texture& texture, sf::Vector2u imgCount, float switchTime);
	AnimateSprite() { currentImage.x = 0; }
public:
	void Update(float deltTime, bool faceRight);
	void ChangeSprite(sf::Texture& texture, sf::Vector2u imgCount, float switchTime);
public:
	sf::IntRect uvRect;
private:
	sf::Vector2u imgCount;
	sf::Vector2u currentImage;

	float totalTime;
	float switchTime;
};

