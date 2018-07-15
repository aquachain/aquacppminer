#pragma once

#include <string>
#include <stdint.h>

struct MiningConfig {
	bool soloMine;
	uint32_t nThreads;
	uint32_t refreshRateMs;

	std::string getWorkUrl;
	std::string submitWorkUrl;
	std::string submitWorkUrl2;
	std::string fullNodeUrl;

	std::string defaultSubmitWorkUrl;
};

void initMiningConfig();
const MiningConfig& miningConfig();

// do not call that during mining, only during init !
void setMiningConfig(MiningConfig cfg);
