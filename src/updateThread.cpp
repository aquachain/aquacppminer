#include <rapidjson/document.h>
#include <curl/curl.h>

#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <assert.h>

#include "miningConfig.h"
#include "miner.h"
#include "updateThread.h"
#include "http.h"
#include "log.h"

using namespace rapidjson;

using std::chrono::high_resolution_clock;

const uint64_t SEND_INITIAL_HASH_RATE_AFTER_S = 30;
const uint64_t HASH_RATE_SEND_INTERVAL_S = 10 * 60;
const uint64_t POLL_INTERVAL_MS = 5 * 1000;

static std::mutex s_hashParams_mutex;
static HashParams s_hashParams;
static bool s_bUpdateThreadRun = true;
bool s_initialHashRateSent = false;

const char* UPDATE_THREAD_LOG_PREFIX = "UPDT";

uint64_t updateThreadPollIntervalMs() {
	return POLL_INTERVAL_MS;
}

#include <atomic>
std::atomic<uint32_t> s_nodeReqId = 0;

static bool updatePoolParams(const MiningConfig& config, HashParams &hashParams, uint32_t hashRate) {
	logLine(UPDATE_THREAD_LOG_PREFIX, "POST REQ");
	char getWorkParams[512];
	snprintf(getWorkParams, sizeof(getWorkParams), "{\"jsonrpc\":\"2.0\", \"id\" : %d, \"method\" : \"aqua_getWork\", \"params\" : null}",
		s_nodeReqId++);
	std::string getWorkResponse;
	bool postRequestOk = httpPostUrlEncoded(config.nodeUrl, getWorkParams, getWorkResponse);

	if (postRequestOk) {
		//logLine(UPDATE_THREAD_LOG_PREFIX, "response:\n%s", getWorkResponse.c_str());
	}
	else {
		logLine(UPDATE_THREAD_LOG_PREFIX, "post req failed");
		return false;
	}

	// result[0], 32 bytes hex encoded current block header pow-hash
	// result[1], 32 bytes hex encoded seed hash used for DAG
	// result[2], 32 bytes hex encoded boundary condition ("target"), 2^256/difficulty

	// parse result
	Document work;
	work.Parse(getWorkResponse.c_str());
	if (!work.IsObject()) {
		return false;
	}

	const char* RESULT = "result";
	if (!work.HasMember(RESULT))
		return false;
	auto resultArray = work[RESULT].GetArray();
	if (resultArray.Size() != 3)
		return false;

	logLine(UPDATE_THREAD_LOG_PREFIX, 
		"\n"
		"block header hash  : %s\n"
		"DAG seed           : %s\n"
		"target             : %s", 
		resultArray[0].GetString(),
		resultArray[1].GetString(),
		resultArray[2].GetString());

	//// JSON helper
	//auto checkAndSetStr = [](rapidjson::GenericValue<rapidjson::UTF8<>>& parent, const std::string &name, std::string &out) -> bool {
	//	if (!parent.HasMember(name.c_str()) || !parent[name.c_str()].IsString())
	//		return false;
	//	out = parent[name.c_str()].GetString();
	//	return true;
	//};

	//// coin must be arionum
	//std::string coin;
	//if (!checkAndSetStr(miningInfo, "coin", coin)) return false;
	//if (coin != "arionum")
	//	return false;

	//// read "data" struct, which contains the mining parameters
	//const char* DATA = "data";
	//if (!miningInfo.HasMember(DATA))
	//	return false;

	//if (!checkAndSetStr(miningInfo[DATA], "difficulty", hashParams.difficulty)) return false;
	//if (!checkAndSetStr(miningInfo[DATA], "block", hashParams.block)) return false;
	//if (!checkAndSetStr(miningInfo[DATA], "public_key", hashParams.publicKey)) return false;

	//const char* LIMIT = "limit";
	//if (!miningInfo[DATA].HasMember(LIMIT) || !miningInfo[DATA][LIMIT].IsInt())
	//	return false;
	//hashParams.limit = miningInfo[DATA][LIMIT].GetUint();

	//const char* HEIGHT = "height";
	//if (!miningInfo[DATA].HasMember(HEIGHT) || !miningInfo[DATA][HEIGHT].IsInt())
	//	return false;
	//hashParams.height = miningInfo[DATA][HEIGHT].GetUint();

	return true;
}

HashParams currentHashParams() {
	HashParams ret;
	s_hashParams_mutex.lock();
	{
		ret = s_hashParams;
	}
	s_hashParams_mutex.unlock();
	return ret;
}

// regularly polls the pool to get new HashParams when block changes
void updateThreadFn() {	
	logLine(UPDATE_THREAD_LOG_PREFIX, "Pool/node update thread launched");
	auto tStart = high_resolution_clock::now();
	uint32_t nHashes = getTotalHashes();

	while (s_bUpdateThreadRun) {
		HashParams newHashParams;

		uint32_t nHashesNow = getTotalHashes();
		auto tNow = high_resolution_clock::now();
		std::chrono::duration<float> durationSinceLast = tNow - tStart;
		bool recomputeHashRate = false;
		if (!s_initialHashRateSent) {
			if (durationSinceLast.count() >= SEND_INITIAL_HASH_RATE_AFTER_S) {
				s_initialHashRateSent = true;
				recomputeHashRate = true;
			}
		}
		else {
			if (durationSinceLast.count() >= HASH_RATE_SEND_INTERVAL_S) {
				recomputeHashRate = true;
			}
		}

		uint32_t hashRate = 0;
		if (recomputeHashRate) {
			hashRate = (nHashesNow - nHashes) / (uint32_t)(durationSinceLast.count());
			tStart = tNow;
			nHashes = nHashesNow;
		}

		bool ok = updatePoolParams(miningConfig(), newHashParams, hashRate);
		if (!ok) {
			logLine(UPDATE_THREAD_LOG_PREFIX, "%s is not responding, retrying in %.2fs",
				miningConfig().nodeUrl.c_str(),
				POLL_INTERVAL_MS/1000.f);
		}
		else {
			if (s_hashParams.height != newHashParams.height) {
				s_hashParams_mutex.lock();
				{
					s_hashParams = newHashParams;
				}
				s_hashParams_mutex.unlock();
				logLine(UPDATE_THREAD_LOG_PREFIX, "New block height: %u difficulty: %s",
					newHashParams.height,
					newHashParams.difficulty.c_str());
			}
#ifdef _DEBUG
			else {
				logLine(UPDATE_THREAD_LOG_PREFIX, "Got pool info: height: %u difficulty: %s",
					newHashParams.height,
					newHashParams.difficulty.c_str());
			}
#endif
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
	}
}

std::thread* s_pThread = nullptr;
void startUpdateThread() {
	if (s_pThread) {
		assert(0);
		return;
	}
	s_pThread = new std::thread(updateThreadFn);
}

void stopUpdateThread() {
	if (s_pThread) {
		assert(s_bUpdateThreadRun);
		s_bUpdateThreadRun = false;
		s_pThread->join();
		delete s_pThread;
	}
	else {
		assert(0);
	}
}