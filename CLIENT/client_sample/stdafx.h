#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <fstream>
#include <array>
#include <unordered_map>
#include <Windows.h>
#include <algorithm>
#include <unordered_set>
#include <chrono>
#include "..\..\SERVER_LUA2\SERVER_LUA\protocol.h"

using namespace std;

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;

constexpr auto TILE_WIDTH = 32;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * 64;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * 64;

extern sf::RenderWindow* g_window;
extern int g_left_x;
extern int g_top_y;
extern int g_myid;
extern sf::Font g_font;
extern sf::Font g_krfont;
