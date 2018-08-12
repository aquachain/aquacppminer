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
#include "hex_encode_utils.h"

#include "../blake2/sse/blake2-config.h"

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

#include <argon2.h>

#ifdef _MSC_VER
	// need to add those libs for windows static linking
	#pragma comment (lib, "crypt32")
	#pragma comment (lib, "Ws2_32")
	#pragma comment (lib, "Wldap32")
	#pragma comment (lib, "Normaliz")
#endif

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
const std::string VERSION = "1.2";

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

std::string getBlakeSSE() {
#if defined(HAVE_SSE41)
	return "SSE4.1";
#elif defined(HAVE_SSSE3)
	return "SSE3";
#elif defined(HAVE_SSE2)
	return "SSE2";
#else
	return "";
#endif
}

std::string getBlakeAVX() {
#if defined(HAVE_AVX)
	return "AVX";
#else
	return "";
#endif
}

std::string getArgon2idAVX() {
	return ARGON_ARCH;
}

void printOptimizationsInfo() {
	auto blakeAVX = std::string(" ") + getBlakeAVX();
	auto blakeSSE = std::string(" ") + getBlakeSSE();
	if (blakeSSE.size() || blakeAVX.size()) {
		printf("-- Blake2b using%s%s\n", blakeAVX.c_str(), blakeSSE.c_str());
	}
	else {
		printf("-- Warning: Blake2b not using any CPU Instructions set (SSE, AVX, ...)\n");
	}

	auto argonAVX = getArgon2idAVX();
	if (argonAVX.size()) {
		printf("-- Argon2id using %s\n", argonAVX.c_str());
	}
	else {
		printf("-- Standard Argon2id (no AVX or AVX2)\n");
	}
}

int main(int argc, char** argv) {
	s_configDir = getPwd(argv);

#ifdef _MSC_VER
	// set a fixed console size (default is not wide enough)
	setConsoleSize(150, 40, 2000);
#endif

	// welcome message
	printf("-- AquaCppMiner %s %s (use -h for help, ctrl+c to quit)\n",
		VERSION.c_str(),
		ARCH);

	printOptimizationsInfo();

	fflush(stdout);

	// random seed
	srand((unsigned int)time(NULL));

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

	// perform tests
#if DO_TESTS
	if (!testAquaHashing()) {
		logLine(COORDINATOR_LOG_PREFIX, "Error: Hashing tests failed !");
		return 1;
	}

	// free any memory used for tests
	freeCurrentThreadMiningMemory();
#endif

	// create & launch update thread
	startUpdateThread();

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
		logLine(COORDINATOR_LOG_PREFIX, "nthreads : %d",
			nThreads);
		logLine(COORDINATOR_LOG_PREFIX, "refresh  : %2.1fs",
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

			auto nSharesSubmitted = getTotalSharesSubmitted();
			auto nSharesAccepted = getTotalSharesAccepted();
			auto nSharesRejected = nSharesSubmitted - nSharesAccepted;
			logLine(COORDINATOR_LOG_PREFIX, "%d threads | %6.2f kH/s | %s=%5lu | Rejected=%5lu (%4.1f%%)", 
				miningConfig().nThreads,
				hashesPerSecondSinceLast / 1000.f,
				miningConfig().soloMine ? "Blocks" : "Shares",
				nSharesAccepted,
				nSharesRejected,
				(nSharesSubmitted == 0) ? 0. : (100. * ((double)nSharesRejected / (double)nSharesSubmitted)));
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