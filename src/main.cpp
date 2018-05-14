#include "miner.h"
#include "tests.h"
#include "miningConfig.h"
#include "updateThread.h"
#include "args.h"
#include "log.h"
#include "config.h"
#include "kbhit.h"
#include "getPwd.h"
#ifdef _MSC_VER
#include "windows/procinfo_windows.h"
#include "windows/win_tools.h"
#endif

#include <assert.h>
#include <openssl/rand.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <stdint.h>
#include <cstring>
#include <stdio.h>

//#ifndef _MSC_VER
//#include <stdio.h>
//#include <termios.h>
//#include <unistd.h>
//#include <fcntl.h>
//#else
//#include <conio.h>
//#define kbhit _kbhit
//#endif

#if defined(__AVX2__)
#pragma message("--- AVX2")
const std::string ARGON_ARCH = "AVX2";
#elif defined(__AVX__)
#pragma message("--- AVX")
const std::string ARGON_ARCH = "AVX";
#else
#pragma message("--- no AVX")
const std::string ARGON_ARCH = "";
#endif

#define DO_TESTS (1)

using std::chrono::high_resolution_clock;

const char* COORDINATOR_LOG_PREFIX = "MAIN";
const std::string VERSION = "0.1";

bool s_needKeyPressAtEnd = false;
bool s_run = true;
std::string s_configDir;

void ctrlCHandler() {
	if (s_run) {
		logLine(COORDINATOR_LOG_PREFIX, "Ctrl+C received, will shutdown soon");
		s_run = false;
	}
}

void initConfigurationFile() {
	std::string confLog;
	if (!createConfigFile(confLog)) {
		logLine(COORDINATOR_LOG_PREFIX, "Warning: configuration file creation failed: %s", confLog.c_str());
	}
}

int main(int argc, char** argv) {
	s_configDir = getPwd(argv);

#ifdef _MSC_VER
	// set a fixed console size (default is not wide enough)
	setConsoleSize(150, 40, 2000);
#endif

	// welcome message
	printf("AquaCppMiner %s %s%s%s\n(use -h for help, ctrl+c to quit)\n", 
		VERSION.c_str(),
		ARGON_ARCH.c_str(),
		(ARGON_ARCH.size() > 0) ? " " : "",
		ARCH);
	fflush(stdout);

	// openssl initialization
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	CONF_modules_load(NULL, "aquacppminer", 0);
	RAND_poll();

	// curl initialization
	CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
	if (res != CURLE_OK) {
		logLine(COORDINATOR_LOG_PREFIX, "Error: Curl init failed, aborting");
		return 1;
	}

	// get cpu hardware infos
#ifdef _MSC_VER
	std::string procInfoLog;
	initProcInfo(procInfoLog);
#endif

	// set default mining config
	initMiningConfig();

	// load or create config file
	std::string confLog;
	if (!configFileExists()) {
		// only launch the config file process if there are no args
		if (argc <= 1) {
			initConfigurationFile();
		}
	}
	else {
		if (!loadConfigFile(confLog)) {
			logLine(COORDINATOR_LOG_PREFIX, "Warning: configuration file seems invalid: %s", confLog.c_str());
			std::cout << "Do you want to create a new config file ? (y/n) ";
			std::string ans;
			std::getline(std::cin, ans);
			if ((ans.size() == 0) || (ans == "y")) {
				initConfigurationFile();
			}
		}
		else {
			logLine(COORDINATOR_LOG_PREFIX, "Configuration file loaded successfully");
		}
	}

	// show number of processors detected
#ifdef _MSC_VER
	logLine(COORDINATOR_LOG_PREFIX, "%s", procInfoLog.c_str());
#endif

	// handle commandline parameters (may change mining config)
	bool argsOk = parseArgs(COORDINATOR_LOG_PREFIX, argc, argv);
	if (!argsOk) {
		return 0;
	}

	// Ctrl+C handler
#ifdef _MSC_VER
	if (!setCtrlCHandler(ctrlCHandler)) {
		logLine(COORDINATOR_LOG_PREFIX, "Error: Could not set ctrl+c handler, aborting");
		return 1;
	}
#endif

	// create & launch update thread
	startUpdateThread();

	// perform tests
#if DO_TESTS
	//std::string hasherLog;
	//bool hasherTestsOk = testHasher(hasherLog);
	//if (!hasherTestsOk) {
	//	logLine(COORDINATOR_LOG_PREFIX, "Error: Hasher tests failed, see error below.");
	//	s_run = false;
	//}
	//logLine(COORDINATOR_LOG_PREFIX, "%s", hasherLog.c_str());

	//std::string submitLog;
	//bool submitTestOk = testSubmit(submitLog);
	//if (!submitTestOk) {
	//	logLine(COORDINATOR_LOG_PREFIX, "Warning: Submit test failed, see message below");
	//}
	//logLine(COORDINATOR_LOG_PREFIX, "%s", submitLog.c_str());

	// free memory used for tests
	//freeCurrentThreadMiningMemory();
#endif

	// auto detect number of threads if not specified
	if (miningConfig().nThreads <= 0) {
		MiningConfig newCfg = miningConfig();
#ifdef _MSC_VER
		newCfg.nThreads = nLogicalCores();
#else
		newCfg.nThreads = std::thread::hardware_concurrency();
#endif
		setMiningConfig(newCfg);
	}

	auto tMiningStart = high_resolution_clock::now();
	auto tLast = tMiningStart;
	uint32_t nHashesLast = 0;
	if (s_run) {
		auto nThreads = miningConfig().nThreads;
		logLine(COORDINATOR_LOG_PREFIX, "--- Start %s mining ---",
			miningConfig().soloMine ? "solo" : "pool");
		logLine(COORDINATOR_LOG_PREFIX, 
			"%-8s : %s", miningConfig().soloMine ? "node" : "pool", 
			miningConfig().getWorkUrl.c_str());
		if (!miningConfig().soloMine && 
			miningConfig().fullNodeUrl.size() > 0) {
			logLine(COORDINATOR_LOG_PREFIX, "node url : %s", 
				miningConfig().fullNodeUrl.c_str());
		}
		logLine(COORDINATOR_LOG_PREFIX,     "nthreads : %d", 
			nThreads);
		logLine(COORDINATOR_LOG_PREFIX,     "refresh  : %2.1fs", 
			miningConfig().refreshRateMs / 1000.0f);
		startMinerThreads(nThreads);
	}

	// run forever until CTRL+C hit
	while (s_run) {
		auto tNow = high_resolution_clock::now();
		uint32_t nHashes = getTotalHashes();
		if (nHashes > 0) {
			// hashes / time since last pass
			std::chrono::duration<float> durationSinceLast = tNow - tLast;
			tLast = tNow;
			assert(nHashes >= nHashesLast);
			auto nHashesSinceLast = nHashes - nHashesLast;
			nHashesLast = nHashes;
			float hashesPerSecondSinceLast = (float)nHashesSinceLast / durationSinceLast.count();

			// hashes / time since start
			std::chrono::duration<float> durationSinceStart = tNow - tMiningStart;
			float hashesPerSecondSinceStart = (float)nHashes / durationSinceStart.count();

			logLine(COORDINATOR_LOG_PREFIX, "working...");

			// log info
			/*auto params = currentHashParams();
			std::string bestStr = getBestStr(params.height);
			if (bestStr == MAX_BEST) {
				bestStr = "N/A";
			}*/

			//mpz_t mpz_poolLimit;
			//mpz_init_set_ui(mpz_poolLimit, params.limit);
			//auto poolLimitStr = heightStr(mpz_poolLimit);

			//logLine(COORDINATOR_LOG_PREFIX, "%5.1f H/s (%2.0fs) | %5.1f H/s (%5s) | h=%u | best=%6s / %3s | %3u shares (%u rej, %u blocks) | %d threads%s",
			//	hashesPerSecondSinceLast,
			//	durationSinceLast.count(),
			//	hashesPerSecondSinceStart,
			//	formatDuration(durationSinceStart.count()).c_str(),
			//	params.height,
			//	bestStr.c_str(),
			//	poolLimitStr.c_str(),
			//	getTotalSharesAccepted(),
			//	getTotalSharesSubmitted() - getTotalSharesAccepted(),
			//	getTotalBlocksAccepted(),
			//	miningConfig().nThreads,
			//	miningConfig().relaxMode ? " (relax)" : "");
		}
		const uint32_t REPORT_INTERVAL_MS = 5 * 1000;
		std::this_thread::sleep_for(std::chrono::milliseconds(REPORT_INTERVAL_MS));
	};

	// kill threads
	logLine(COORDINATOR_LOG_PREFIX, "Stopping Threads");
	stopMinerThreads();
	stopUpdateThread();

	// curl shutdown
	curl_global_cleanup();

	// openssl shutdown
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();

	logLine(COORDINATOR_LOG_PREFIX, "Goodbye !");

	if (s_needKeyPressAtEnd) {
		printf("Press any key to exit...");
		while (!kbhit()) {
		}
	}

	return 0;
}