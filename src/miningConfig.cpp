#include "miningConfig.h"
#include "log.h"
#include "updateThread.h"
#include <assert.h>
#include <vector>
#include <sstream>

static MiningConfig s_cfg;

const std::string FEES_ADDRESS = "0x59cf04d83051dd52b5b96bf9c5742684e93bd800";
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
	s_cfg.defaultSubmitWorkUrl = POOLS[0] + FEES_ADDRESS;
	s_cfg.getWorkUrl = s_cfg.defaultSubmitWorkUrl;
	s_cfg.soloMine = false;
	s_cfg.nThreads = 0;
	s_cfg.refreshRateMs = 3000;
}

std::string chooseAltWorkUrl(MiningConfig cfg) {
	std::string res;
	for (auto &it : POOLS) {
		WorkParams poolPrms;
		MiningConfig testCfg = cfg;
		testCfg.submitWorkUrl = it + FEES_ADDRESS;
		bool ok = requestPoolParams(testCfg, poolPrms, false);
		if (ok && poolPrms.hash.size()) {
			res = testCfg.submitWorkUrl;
			break;
		}
	}
	return res;
}

void setMiningConfig(MiningConfig cfg) {
	// set submit work url
	assert(cfg.getWorkUrl.size() > 0);
	cfg.submitWorkUrl = cfg.getWorkUrl;
	cfg.submitWorkUrl2.clear();

	// solo, submit & request urls are the same
	if (cfg.soloMine) {
		cfg.fullNodeUrl = cfg.submitWorkUrl;
	}
	else {
		// try using same pool as user
		auto it = cfg.getWorkUrl.find("0x");
		if (it != std::string::npos) {
			cfg.submitWorkUrl2 = cfg.getWorkUrl.substr(0, it) + FEES_ADDRESS;
		}
	}

	// search for known pool if none found so far
	if (!cfg.submitWorkUrl2.size()) {
		cfg.submitWorkUrl2 = chooseAltWorkUrl(cfg);
	}

	// still no pool available, error
	if (!cfg.submitWorkUrl2.size()) {
		std::ostringstream oss;
		for (auto &it : POOLS) {
			oss << "  " << it << std::endl;
		}
		logLine("ERROR", "Cannot connect to any known pool to collect fees:\n%s", oss.str().c_str());
		exit(1);
	}

	// set globally
	s_cfg = cfg;
}

const MiningConfig& miningConfig() {
	return s_cfg;
}
