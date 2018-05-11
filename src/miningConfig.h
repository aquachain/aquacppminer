#pragma once

#include <string>
#include <stdint.h>

struct MiningConfig {
	bool soloMine;
	uint32_t nThreads;
	uint32_t refreshRateMs;
	std::string nodeUrl;
	std::string devPoolUrl;
};

void initMiningConfig();
const MiningConfig& miningConfig();

// do not call that during mining, only during init !
void setMiningConfig(const MiningConfig& cfg);
