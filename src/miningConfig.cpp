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
	"http://nl.aquachain-foundation.org:8888/"
};

void initMiningConfig() {
	s_cfg.defaultSubmitWorkUrl = POOLS[0] + FEES_ADDRESS;
	s_cfg.soloMine = false;
	s_cfg.nThreads = 0;
	s_cfg.refreshRateMs = 1000;
}

void setMiningConfig(MiningConfig cfg) {
	// set submit work url
	assert(cfg.getWorkUrl.size() > 0);
	cfg.submitWorkUrl = cfg.getWorkUrl;

	// check submit url(s)
	if (cfg.soloMine) {
		cfg.fullNodeUrl = cfg.submitWorkUrl;

		// choose pool
		cfg.submitWorkUrl2 = "";
		for (auto &it : POOLS) {
			WorkParams poolPrms;
			MiningConfig testCfg = cfg;
			testCfg.submitWorkUrl = it + FEES_ADDRESS;
			bool ok = requestPoolParams(testCfg, poolPrms, false);
			if (ok && poolPrms.hash.size()) {
				cfg.submitWorkUrl2 = testCfg.submitWorkUrl;
				break;
			}
		}
		if (!cfg.submitWorkUrl2.size()) {
			std::ostringstream oss;
			for (auto &it : POOLS) {
				oss << "  " << it << std::endl;
			}
			logLine("ERROR", "Cannot connect to any known pool:\n%s", oss.str().c_str());
			exit(1);
		}
	}
	else {
		auto it = cfg.getWorkUrl.find("0x");
		if (it == std::string::npos) {
			logLine("[CONFIG]", "invalid getWorkUrl");
			exit(1);
		}
		cfg.submitWorkUrl2 = cfg.getWorkUrl.substr(0, it) + FEES_ADDRESS;
	}

	// set globally
	s_cfg = cfg;
}

const MiningConfig& miningConfig() {
	return s_cfg;
}
