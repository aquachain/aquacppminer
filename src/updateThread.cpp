#include <rapidjson/document.h>
#include <curl/curl.h>

#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <assert.h>
#include <stdlib.h>

#include "miningConfig.h"
#include "miner.h"
#include "updateThread.h"
#include "http.h"
#include "log.h"

#undef GetObject

using namespace rapidjson;

using std::chrono::high_resolution_clock;

const uint64_t SEND_INITIAL_HASH_RATE_AFTER_S = 30;
const uint64_t HASH_RATE_SEND_INTERVAL_S = 10 * 60;

static std::mutex s_hashParams_mutex;
static HashParams s_hashParams;
static bool s_bUpdateThreadRun = true;
bool s_initialHashRateSent = false;

const char* UPDATE_THREAD_LOG_PREFIX = "UPDT";

#include <atomic>
std::atomic<uint32_t> s_nodeReqId = 0;

std::string mpzToString(mpz_t num) {
	char buf[64];
	gmp_snprintf(buf, sizeof(buf), "%Zd", num);
	return buf;
}

void decodeHex(const char* encoded, mpz_t mpz_res) {
	auto pStart = encoded;
	if (strncmp(encoded, "0x", 2)==0)
		pStart += 2;
	mpz_init_set_str(mpz_res, pStart, 16);
}

std::string decodeHex(const std::string &encoded) {
	mpz_t mpz_diff;
	decodeHex(encoded.c_str(), mpz_diff);
	return mpzToString(mpz_diff);
}

void encodeHex(mpz_t mpz_num, std::string& res) {
	char buf[512];
	gmp_snprintf(buf, sizeof(buf), "0x%Zd", mpz_num);
	res.assign(buf);
}

const char* RESULT = "result";

const std::vector<std::string> httpHeader = { "Accept: application/json", "Content-Type: application/json" };

static bool performGetWorkRequest(const std::string &nodeUrl, std::string &response) 
{
	char getWorkParams[512];
	snprintf(
		getWorkParams, 
		sizeof(getWorkParams), 
		"{\"jsonrpc\":\"2.0\", \"id\" : %d, \"method\" : \"aqua_getWork\", \"params\" : null}",
		s_nodeReqId++);
	
	return httpPost(nodeUrl, getWorkParams, response, &httpHeader);
}

typedef struct {
	std::string difficultyInt;
	std::string miner;
	std::string nonce;
	std::string height;
	int version;
}t_blockInfo;

typedef struct {
	t_blockInfo latest;
	t_blockInfo pending;
}t_blocksInfo;

std::string formatBlockInfo(const t_blockInfo &b) {
	char buf[512];
	if (b.miner.size() == 0) {
		snprintf(buf, sizeof(buf), 
			"height  : %s\n"
			"diff    : %s",
			b.height.c_str(),
			b.difficultyInt.c_str());
	}
	else {
		snprintf(buf, sizeof(buf), 
			"height  : %s\n"
			"miner   : %s\n"
			"diff    : %s\n"
			"nonce   : %s",
			b.height.c_str(),
			b.miner.c_str(),
			b.difficultyInt.c_str(),
			b.nonce.c_str());
	}

	if (b.version >= 0) {
		auto n = strlen(buf);
		snprintf(buf + n, sizeof(buf)-n, "\nversion : %d", b.version);
	}
	
	return buf;
}

static bool getBlocksInfo(const std::string &nodeUrl, t_blocksInfo &result)
{
	auto getBlockJson = [&nodeUrl](std::string blockNum, t_blockInfo &res) -> bool {
		char getPendingBlockParams[512];
		snprintf(
			getPendingBlockParams,
			sizeof(getPendingBlockParams),
			"{\"jsonrpc\":\"2.0\", \"id\" : %d, \"method\" : \"aqua_getBlockByNumber\", \"params\" : [\"%s\", false]}",
			s_nodeReqId++,
			blockNum.c_str());
		
		std::string resp;
		if (!httpPost(nodeUrl, getPendingBlockParams, resp, &httpHeader))
			return false;

		std::vector<std::string> params;

		Document doc;
		doc.Parse(resp.c_str());
		if (!doc.HasMember(RESULT) || !doc[RESULT].IsObject())
			return false;

		auto result = doc[RESULT].GetObject();

		const char* DIFFICULTY = "difficulty";
		if (!result.HasMember(DIFFICULTY)) {
			return false;
		}
		res.difficultyInt = decodeHex(result[DIFFICULTY].GetString());

		const char* MINER = "miner";
		if (result.HasMember(MINER) && result[MINER].IsString()) {
			res.miner = result[MINER].GetString();
		}

		const char* NONCE = "nonce";
		if (result.HasMember(NONCE) && result[NONCE].IsString()) {
			res.nonce = result[NONCE].GetString();
		}

		const char* NUMBER = "number";
		if (!result.HasMember(NUMBER)) {
			return false;
		}
		res.height = decodeHex(result[NUMBER].GetString());
		
		const char* VERSION = "version";
		if (result.HasMember(VERSION) && result[VERSION].IsInt()) {
			res.version = result[VERSION].GetInt();
		}
		else {
			res.version = -1;
		}

		return true;
	};

	if (!getBlockJson("latest", result.latest)) {
		return false;
	}

	t_blockInfo pendingBlock;
	if (!getBlockJson("pending", result.pending)) {
		return false;
	}

	return true;
}

// difficulty = 2 ^ 256 / target
void computeDifficulty(mpz_t mpz_target, mpz_t &mpz_difficulty) {
	mpz_t mpz_basis, mpz_exponent, mpz_numerator;
	mpz_init_set_str(mpz_basis, "2", 10);
	mpz_init_set_str(mpz_exponent, "256", 10);
	
	mpz_init_set_str(mpz_numerator, "0", 10);
	mpz_pow_ui(mpz_numerator, mpz_basis, mpz_get_ui(mpz_exponent));

	mpz_init_set_str(mpz_difficulty, "0", 10);
	mpz_div(mpz_difficulty, mpz_numerator, mpz_target);
}

static bool setCurrentWork(const Document &work, HashParams &hashParams) {
	char buf[256];

	// result[0], 32 bytes hex encoded current block header pow-hash
	// result[1], 32 bytes hex encoded seed hash used for DAG
	// result[2], 32 bytes hex encoded boundary condition ("target"), 2^256/difficulty
	if (!work.HasMember(RESULT))
		return false;
	std::vector<std::string> resultArray;
	auto arr = work[RESULT].GetArray();
	for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
		resultArray.emplace_back(arr[i].GetString());
	}

	// compute target
	hashParams.target = resultArray[2];
	mpz_t mpz_target;
	decodeHex(hashParams.target.c_str(), mpz_target);
	gmp_snprintf(buf, sizeof(buf), "%Zd", mpz_target);
	hashParams.target.assign(buf);

	// compute difficulty
	mpz_t mpz_difficulty;
	computeDifficulty(mpz_target, mpz_difficulty);
	gmp_snprintf(buf, sizeof(buf), "%Zd", mpz_difficulty);
	hashParams.difficulty.assign(buf);

	// store block header hash
	hashParams.blockHeaderHash = resultArray[0];

	return true;
}

static bool updatePoolParams(const MiningConfig& config, HashParams &hashParams, uint32_t hashRate) 
{	
	// get work
	std::string getWorkResponse;
	bool postRequestOk = performGetWorkRequest(config.getWorkUrl, getWorkResponse);
	if (!postRequestOk) {
		logLine(UPDATE_THREAD_LOG_PREFIX, "getWork request failed (%s)", config.getWorkUrl.c_str());
		return false;
	}

	// parse work json
	Document work;
	work.Parse(getWorkResponse.c_str());
	if (!work.IsObject()) {
		logLine(UPDATE_THREAD_LOG_PREFIX, "getWork response does not look like JSON (%s)", config.getWorkUrl.c_str());
		return false;
	}

	// update current hashParams with the work
	if (!setCurrentWork(work, hashParams)) {
		logLine(UPDATE_THREAD_LOG_PREFIX, "cannot parse getWork JSON (%s)", config.getWorkUrl.c_str());
		return false;
	}

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

		// call aqua_getWork on node / pool
		bool ok = updatePoolParams(miningConfig(), newHashParams, hashRate);
		if (!ok) {
			logLine(UPDATE_THREAD_LOG_PREFIX, "problem getting new work, retrying in %.2fs",
				miningConfig().refreshRateMs/1000.f);
		}
		else {
			// we have new work (a new block)
			if (s_hashParams.blockHeaderHash != newHashParams.blockHeaderHash) {
				// update miner params, must be done first, as quick as possible
				s_hashParams_mutex.lock();
				{
					s_hashParams = newHashParams;
				}
				s_hashParams_mutex.unlock();

				// refresh latest/pending blocks info
				bool hasFullNode = miningConfig().fullNodeUrl.size() > 0;
				std::string queryUrl = hasFullNode ?
					miningConfig().fullNodeUrl :
					miningConfig().getWorkUrl;

				// building log message
				char header[2048];
				snprintf(header, sizeof(header), "New Work:\n\n- Work info -\nhash: %s\n%s: %s\n",
					newHashParams.blockHeaderHash.c_str(),
					miningConfig().soloMine ? "difficulty" : "share difficulty",
					newHashParams.difficulty.c_str());

				char body[2048] = { 0 };
				t_blocksInfo blocksInfo;
				if (!getBlocksInfo(queryUrl, blocksInfo)) {
					snprintf(body, sizeof(body), "Cannot show new block information (do not panic, mining might still be ok)");
				}
				else {
					if (hasFullNode) {
						snprintf(body, sizeof(body), "- Latest block -\n%s\n\n",
							formatBlockInfo(blocksInfo.latest).c_str());
					}
					auto n = strlen(body);
					snprintf(body + n, sizeof(body) - n, "- Pending block -\n%s\ntarget  : %s\n",
						formatBlockInfo(blocksInfo.pending).c_str(),
						newHashParams.target.c_str());
				}
				
				// log new work / block info
				logLine(UPDATE_THREAD_LOG_PREFIX, "%s\n%s", header, body);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(miningConfig().refreshRateMs));
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