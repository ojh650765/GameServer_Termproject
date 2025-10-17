#pragma once
#include "stdafx.h"

class TiledMap
{
public:
	TiledMap();
	static TiledMap& getInstance()
	{
		static TiledMap instance;
		if (!instance.initialized) {
			instance.initialized = true;
		}
		return instance;
	}
public:
	bool isCollision(int x, int y) const;
private:   
	struct Tile {
		int gid;
		int x;
		int y;
	};
	std::vector<std::vector<uint8_t>> m_collision;
	   std::vector<uint8_t> metaData;
	  
	   int m_mapWidth;
	   int m_mapHeight;
	   int m_tileWidth;
	   int m_originalWidth;
	   int m_originalHeight;
	   int m_tileHeight;
	   bool initialized{};
};

