#include "AnimateSprite.h"

AnimateSprite::AnimateSprite(sf::Texture& texture, sf::Vector2u imgCount, float switchTime)
	:imgCount{ imgCount }, switchTime{ switchTime }, totalTime{}
{
	currentImage.x = 0;
	currentImage.y = 1;
	uvRect.width = texture.getSize().x / float(imgCount.x);
	uvRect.height = texture.getSize().y / float(imgCount.y);
}

void AnimateSprite::Update(float deltTime, bool faceRight)
{
	totalTime += deltTime;
	if (totalTime >= switchTime) {
		totalTime -= switchTime;
		++currentImage.x;
		if (currentImage.x >= imgCount.x) {
			currentImage.x = 0;
		}
	}

	uvRect.top = currentImage.y * uvRect.height;
	if (faceRight) {
		uvRect.left = currentImage.x * uvRect.width;
		uvRect.width = abs(uvRect.width);
	}
	else {
		uvRect.left = (currentImage.x + 1) * abs(uvRect.width);
		uvRect.width = -abs(uvRect.width);
	}
}

void AnimateSprite::ChangeSprite(sf::Texture& texture, sf::Vector2u imgCount, float switchTime)
{
	currentImage.x = 0;
	uvRect.width = texture.getSize().x / float(imgCount.x);
	uvRect.height = texture.getSize().y / float(imgCount.y);
	this->imgCount = imgCount;
	this->switchTime = switchTime; 
	this->totalTime = 0;
}
