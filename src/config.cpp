#include "miningConfig.h"
#include "config.h"
#include "args.h"
#include "string_utils.h"

#ifdef _MSC_VER
#include "windows/procinfo_windows.h"
#endif

#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <cstring>

extern std::string s_configDir;

const std::string CONFIG_FILE_NAME = "config.cfg";

std::string readInput() {
	std::string res;
	std::getline(std::cin, res);
	res = trim(res);
	return res;
}

std::string configFilePath()
{
	return s_configDir + CONFIG_FILE_NAME;
}

enum {
	MODE = 0,
	GET_WORK_URL,
	NTHREADS,
	REFRESH_RATE_MS,
	FULLNODE_URL,
	N_PARAMS
};

bool configFileExists() {
	std::ifstream fs(configFilePath());
	if (!fs.is_open())
		return false;
	return true;
}

bool loadConfigFile(std::string& log) {	
	MiningConfig newCfg = miningConfig();

	std::ifstream fs(configFilePath());
	const size_t BUFLEN = 256;
	char params[N_PARAMS][BUFLEN];
	for (int i = 0; i < N_PARAMS; i++) {
		if (!fs.getline(params[i], BUFLEN)) {
			log = "not enough lines";
			return false;
		}
		if (i != FULLNODE_URL && (params[i] == nullptr)) {
			log = "empty param";
			return false;
		}
	}

	if (!strcmp(params[MODE], "pool")) {
		newCfg.soloMine = false;
	}
	else if (!strcmp(params[MODE], "solo")) {
		newCfg.soloMine = true;
	}
	else {
		log = "cannot find solo/pool mode";
		return false;
	}
		
	newCfg.getWorkUrl = params[GET_WORK_URL];
	newCfg.fullNodeUrl = params[FULLNODE_URL];

	if (sscanf(params[NTHREADS], "%d", &newCfg.nThreads) != 1) {
		log = "cannot find thread count";
		return false;
	}
	
	if (sscanf(params[REFRESH_RATE_MS], "%u", &newCfg.refreshRateMs) != 1) {
		log = "cannot find refresh rate";
		return false;
	}

	setMiningConfig(newCfg);
	return true;
}

bool createConfigFile(std::string &log) {
	MiningConfig newCfg = miningConfig();
	
	std::cout << std::endl << "-- Configuration File creation --" << std::endl;
	
	bool modeOk = false;
	while (!modeOk) {
		std::cin.clear();
		std::cout << "pool or solo mine ? (pool/solo) ";
		std::string modeStr;
		std::getline(std::cin, modeStr);
		modeOk = true;
		if (modeStr == "pool") {
			newCfg.soloMine = false;
		}
		else if (modeStr == "solo") {
			newCfg.soloMine = true;
		}
		else {
			modeOk = false;
			std::cout << "Please answer pool or solo." << std::endl;
		}
	}

	bool getWorkUrlOk = false;
	while (!getWorkUrlOk) {
		std::cin.clear();
		std::cout << 
			(newCfg.soloMine ?
				"Enter node url (ex: http://127.0.0.1:8543)" :
				"Enter pool url (ex: http://pool.aquachain-foundation.org:8888/0x1d23de...)")
				<< ", if empty, will pool mine to dev wallet): " << std::endl;
		std::getline(std::cin, newCfg.getWorkUrl);
		newCfg.getWorkUrl = trim(newCfg.getWorkUrl);
		if (newCfg.getWorkUrl.size() == 0) {
			newCfg.getWorkUrl = miningConfig().defaultSubmitWorkUrl;
			newCfg.soloMine = false;
		}
		getWorkUrlOk = true;
	}

	if (newCfg.soloMine) {
		newCfg.fullNodeUrl = newCfg.getWorkUrl;
	}
	else {
		bool fullNodeUrlOk = false;
		while (!fullNodeUrlOk) {
			std::cin.clear();
			std::cout <<
				"Enter node url, ex: http://127.0.0.1:8543 (optional, enter to skip):"
				<< std::endl;
			newCfg.fullNodeUrl = readInput();
			fullNodeUrlOk = true;
		}
	}

	bool nThreadsOk = false;
	while (!nThreadsOk) {
		std::cin.clear();
		std::cout << "Enter number of threads to use, 0/empty for auto (" << std::thread::hardware_concurrency() << " cores detected): ";
		std::string nThreadsStr;
		std::getline(std::cin, nThreadsStr);
		if (nThreadsStr.size() == 0) {
			nThreadsOk = true;
			newCfg.nThreads = miningConfig().nThreads;
		}
		else {
			int nThreads = 0;
			nThreadsOk = sscanf(nThreadsStr.c_str(), "%d", &nThreads) == 1;
			if (nThreads >= 0) {
				newCfg.nThreads = (uint32_t)nThreads;
			}
			else {
				nThreadsOk = false;
			}
		}
		if (!nThreadsOk) {
			std::cout << "invalid number of threads" << std::endl;
			return false;
		}
	}

	bool refreshRateOk = false;
	while (!refreshRateOk) {
		std::cin.clear();
		std::cout << "Enter refresh rate (ex: 1s, 2.5m) " << std::endl;
		std::string refreshRateStr;
		std::getline(std::cin, refreshRateStr);
		auto res = parseRefreshRate(refreshRateStr);
		if (res.first) {
			newCfg.refreshRateMs = res.second;
			refreshRateOk = true;
		}
		else {
			std::cout << "cannot parse refresh rate" << std::endl;
		}
	}

	std::ofstream fs(configFilePath());
	if (!fs.is_open()) {
		log = "Cannot open config file for writing";
		return false;
	}

	fs << (newCfg.soloMine ? "solo" : "pool") << std::endl;
	fs << newCfg.getWorkUrl << std::endl;
	fs << newCfg.nThreads << std::endl;
	fs << newCfg.refreshRateMs << std::endl;
	fs << newCfg.fullNodeUrl << std::endl;

	fs.close();
	if (!fs) {
		std::cout << "cannot write to " << CONFIG_FILE_NAME << std::endl;
		return false;
	}

	std::cout << std::endl << "-- Configuration written to " << CONFIG_FILE_NAME << " --" << std::endl;
	std::cout << "To change config later, either edit " << CONFIG_FILE_NAME << " or delete it and relaunch the miner" << std::endl;
	std::cout << std::endl;

	setMiningConfig(newCfg);

	return true;
}
