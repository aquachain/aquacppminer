#include "miningConfig.h"
#include "log.h"
#include "updateThread.h"
#include <assert.h>
#include <vector>
#include <sstream>

static MiningConfig s_cfg;

// TODO: read from https://aquachain.github.io/pools.json
const std::vector<std::string> POOLS = {
	"http://aqua.signal2noi.se:19998/",
	"http://pool.aquachain-foundation.org:8888/",
	"http://nl.aquachain-foundation.org:8888/",
	"http://aquacha.in:8888/",
	"http://aquapool.rplant.xyz:19998/",
	"http://aqua.cnpool.vip:8888/",
	"http://aqua.dapool.me:3333/"
};

void initMiningConfig() {
	s_cfg.defaultSubmitWorkUrl = "http://127.0.0.1:8543";
	s_cfg.getWorkUrl = s_cfg.defaultSubmitWorkUrl;
	s_cfg.soloMine = false;
	s_cfg.nThreads = 0;
	s_cfg.refreshRateMs = 3000;
}


void setMiningConfig(MiningConfig cfg) {
	// set submit work url
	assert(cfg.getWorkUrl.size() > 0);
	cfg.submitWorkUrl = cfg.getWorkUrl;

	// solo, submit & request urls are the same
	if (cfg.soloMine) {
		cfg.fullNodeUrl = cfg.submitWorkUrl;
	}

	// set globally
	s_cfg = cfg;
}

const MiningConfig& miningConfig() {
	return s_cfg;
}
