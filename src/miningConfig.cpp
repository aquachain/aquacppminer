#include "miningConfig.h"
#include "uniqid.h"

static MiningConfig s_cfg;

void initMiningConfig() {
	s_cfg.devPoolUrl = "http://pool.aquachain-foundation.org:8888/0x3847b2785fad1877c064cd259498ea2b5bffc01d";
	s_cfg.soloMine = false;
	s_cfg.nThreads = 0;
	s_cfg.refreshRateMs = 1000;
}

void setMiningConfig(const MiningConfig& cfg) {
	s_cfg = cfg;
}

const MiningConfig& miningConfig() {
	return s_cfg;
}
