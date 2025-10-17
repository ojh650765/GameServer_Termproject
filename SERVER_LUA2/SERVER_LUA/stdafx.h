#pragma once
#include <iostream>
#include <fstream>

#include <WS2tcpip.h>
#include <MSWSock.h>

#include <thread>
#include <chrono>
#include <mutex>

#include <concurrent_priority_queue.h>
#include <vector>
#include <array>
#include <unordered_set>
#include <concurrent_queue.h>
#include <queue>

#include <utility>
#include <sql.h>
#include <sqlext.h>
#include <algorithm>
#include <string>
#include <random>
#include "include/lua.hpp"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")

enum EVENT_TYPE { EV_RANDOM_MOVE, EV_RUN_AWAY, EV_RESURRECTION, EV_HEAL};


struct EVENT {
	int	obj_id;
	std::chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE et;
	int target_obj;
	constexpr bool operator < (const EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};


struct QueryTask {
	int index;
	char client_name[20];
	int type;
};
