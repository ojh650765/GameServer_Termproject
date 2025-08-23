#pragma once
#include "../../include/json.hpp"
#include "stdafx.h"

using json = nlohmann::json;

class TileMap {
public:
    TileMap(const std::string& mapFile, const std::string& tilesetFile)
        : m_mapFile(mapFile), m_tilesetFile(tilesetFile) {
        load();
    }
    static TileMap& getInstance(const std::string& mapFile = "", const std::string& tilesetFile = "") {
        static TileMap instance(mapFile, tilesetFile);
        return instance;
    }   
    bool loaded{};
    bool load()
    {
        if (!loaded) {
            std::ifstream file(m_mapFile);
            if (!file.is_open()) {
                std::cerr << "Failed to open map file." << std::endl;
                return false;
            }

            json j;
            file >> j;

            m_tileWidth = j["tilewidth"];
            m_tileHeight = j["tileheight"];

            // 원래 맵 크기
            m_originalWidth = j["width"];
            m_originalHeight = j["height"];

            // 레이어 데이터 로드
            std::vector<int> originalData;
            for (const auto& layer : j["layers"]) {
                if (layer["type"] == "tilelayer" && layer["name"] == "Tile Layer 1") {
                    for (auto gid : layer["data"]) {
                        originalData.push_back(gid);
                    }
                }
                if (layer["name"] == "Meta") {
                    for (auto gid : layer["data"]) {
                        metaData.push_back(gid);
                    }
                }
            }
            m_mapWidth = 3000;
            m_mapHeight = 3000;

            m_tiles.resize(m_mapHeight, std::vector<int>(m_mapWidth, 0));
            m_collision.resize(m_mapHeight, std::vector<uint8_t>(m_mapWidth, false));

            for (int y = 0; y < m_mapHeight; ++y) {
                for (int x = 0; x < m_mapWidth; ++x) {
                    int origX = x % m_originalWidth;
                    int origY = y % m_originalHeight;
                    m_tiles[y][x] = originalData[origY * m_originalWidth + origX];
                    m_collision[y][x] = metaData[origY * m_originalWidth + origX];
                }
            }
            m_tilesetTexture = new sf::Texture();

            if (!m_tilesetTexture->loadFromFile(m_tilesetFile)) {
                std::cerr << "Failed to load tileset image." << std::endl;
                return false;
            }


            TextureXSize = m_tilesetTexture->getSize().x;
            m_tileSprite.setTexture(*m_tilesetTexture);
            return true;
        }
        loaded = true;
    }
    void draw(sf::RenderWindow& window, int offsetX, int offsetY)
    {
        int startX = offsetX / (m_tileWidth);
        int startY = offsetY / (m_tileHeight);
        int endX = std::min((int)(startX + window.getSize().x / m_tileWidth + 1), W_WIDTH);
        int endY = std::min((int)(startY + window.getSize().y / m_tileHeight + 1), W_WIDTH);
        for (int y = startY; y < endY; ++y) {
            for (int x = startX; x < endX; ++x) {

                if (x >= 0 && y >= 0) {
                    int gid = m_tiles[y][x];
                    if (gid > 0) {
                     
                        int tilesetColumns = TextureXSize / (m_tileWidth + 1);
                        if (tilesetColumns > 0) {

                            int tu = (gid - 1) % tilesetColumns;
                            int tv = (gid - 1) / tilesetColumns;

                            m_tileSprite.setTextureRect(sf::IntRect(
                                tu * (m_tileWidth + 1) + 1, // 좌측 1픽셀 오프셋
                                tv * (m_tileHeight + 1) + 1, // 상단 1픽셀 오프셋
                                m_tileWidth,
                                m_tileHeight

                            ));
                        }
                        m_tileSprite.setPosition((x * m_tileWidth) - offsetX, (y * m_tileHeight) - offsetY);

                        window.draw(m_tileSprite);
                    }
                }
            }
        }
    }
    int getMapWidth() const { return m_mapWidth; }
    int getMapHeight() const { return m_mapHeight; }
    bool isCollision(int x, int y) const
    {
        if (x < 0 || y < 0 || x > W_WIDTH || y > W_WIDTH) {
            return false;
        }

        return (m_collision[y][x] != 0);
    }


private:
    struct Tile {
        int gid;
        int x;
        int y;
    };

    std::string m_mapFile;
    std::string m_tilesetFile;
    sf::Texture* m_tilesetTexture;
    sf::Sprite m_tileSprite;
    int m_mapWidth;
    int m_mapHeight;
    int m_tileWidth;
    int m_originalWidth;
    int m_originalHeight;
    int m_tileHeight;
    float TextureXSize;
    std::vector<std::vector<int>> m_tiles;
    std::vector<std::vector<uint8_t>> m_collision;
    std::vector<uint8_t> metaData;
};
