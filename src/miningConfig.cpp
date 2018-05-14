#include "miningConfig.h"
#include <assert.h>

static MiningConfig s_cfg;

void initMiningConfig() {
	s_cfg.defaultSubmitWorkUrl = "http://pool.aquachain-foundation.org:8888/0x3847b2785fad1877c064cd259498ea2b5bffc01d";
	s_cfg.soloMine = false;
	s_cfg.nThreads = 0;
	s_cfg.refreshRateMs = 1000;
}

void setMiningConfig(MiningConfig cfg) {
	// set submit work url
	assert(cfg.getWorkUrl.size() > 0);
	cfg.submitWorkUrl = cfg.getWorkUrl;

	// set globally
	s_cfg = cfg;
}

const MiningConfig& miningConfig() {
	return s_cfg;
}
