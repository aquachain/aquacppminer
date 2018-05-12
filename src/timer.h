#pragma once

#include <chrono>

struct Timer {
	void start();
	void end(float &tgt);
	std::chrono::time_point<std::chrono::high_resolution_clock> startT, endT;
};