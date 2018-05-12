#pragma once

#include "miner.h"

void startUpdateThread();
void stopUpdateThread();

HashParams currentHashParams();
uint64_t updateThreadPollIntervalMs();
