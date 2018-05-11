#include "args.h"
#include "inputParser.h"
#include "miningConfig.h"
#include "log.h"

#include <assert.h>

const std::string OPT_USAGE = "-h";
const std::string OPT_NTHREADS = "-t";
const std::string OPT_NODE_URL = "-F";
const std::string OPT_REFRESH_RATE = "-r";
const std::string OPT_SOLO = "--solo";

static const std::string s_usageMsg = 
"--- Usage ---\n"
"aquacppminer.exe    [-F pool/node address] [-t nThreads] [--solo] [-h]\n"
"  -t nThreads         : number of threads to use (if not specified will use maximum logical threads available)\n"
"  -F url              : url of pool or node (solo mining), if not specified, will pool mine to dev's aquabase\n"
"  -r                  : refresh rate, ex: 1s, 2.5m\n"
"  --solo              : solo mining, if not specified default is pool mining\n"
"  -h                  : display this help message and exit\n"
"\n"
;

void printUsage()
{
	printf("%s\n", s_usageMsg.c_str());
}

std::pair<bool, uint32_t> parseRefreshRate(const std::string& refreshRateStr) {
	char unit[8];
	float refreshRate;
	int count = sscanf_s(refreshRateStr.c_str(), "%f%s", &refreshRate, unit, 8);
	if (count != 2 || (unit[0] != 'm' && unit[0] != 's')) {
		return { false, 0 };
	}
	if (unit[0] == 's') {
		return { true, uint32_t(1000.f * refreshRate) };
	}
	else if (unit[0] == 'm') {
		return { true, uint32_t(1000.f * 60.f * refreshRate) };
	}
	assert(0);
	return{ false, 0 };
}

bool parseArgs(const char* prefix, int argc, char** argv)
{
	InputParser ip(argc, argv);
	MiningConfig cfg = miningConfig();
	
	if (ip.cmdOptionExists(OPT_USAGE)) {
		printUsage();
		return false;
	}

	if (ip.cmdOptionExists(OPT_NTHREADS)) {
		const auto& nThreadsStr = ip.getCmdOption(OPT_NTHREADS);
		int n = sscanf(nThreadsStr.c_str(), "%u", &cfg.nThreads);
		if (n<1) {
			logLine(prefix, 
				"Warning: invalid value for number of threads (%d), reverting to default", 
				cfg.nThreads);
			cfg.nThreads = 0;
		}
	}

	if (ip.cmdOptionExists(OPT_NODE_URL)) {
		cfg.nodeUrl = ip.getCmdOption(OPT_NODE_URL);
	}

	if (ip.cmdOptionExists(OPT_REFRESH_RATE)) {
		char unit[8];
		float refreshRate;
		std::string refreshStr = ip.getCmdOption(OPT_REFRESH_RATE);
		auto res = parseRefreshRate(refreshStr);
		if (!res.first)
			return false;
		cfg.refreshRateMs = res.second;
	}

	if (ip.cmdOptionExists(OPT_SOLO)) {
		cfg.soloMine = true;
	}

	setMiningConfig(cfg);

	return true;
}
