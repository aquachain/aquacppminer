#pragma once

#include "miner.h"
#include "miningConfig.h"

void startUpdateThread();
void stopUpdateThread();

WorkParams currentWorkParams();
bool requestPoolParams(const MiningConfig& config, WorkParams &workParams, bool verbose);
uint32_t getPoolGetWorkCount();
