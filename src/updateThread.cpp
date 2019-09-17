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
#include "hex_encode_utils.h"

#include <atomic>
#include <map>

#undef GetObject

using namespace rapidjson;

using std::chrono::high_resolution_clock;

const char* UPDATE_THREAD_LOG_PREFIX = "UPDT";
const char* RESULT = "result";
const std::vector<std::string> HTTP_HEADER = { 
	"Accept: application/json", 
	"Content-Type: application/json" 
};

static bool s_bUpdateThreadRun = true;
std::mutex s_workParams_mutex;
static WorkParams s_workParams;
std::atomic<uint32_t> s_nodeReqId = { 0 };
std::atomic<uint32_t> s_poolGetWorkCount = { 0 }; // number of succesfull getWork done so far

static std::map<std::string, http_connection_handle_t> s_httpHandles;

uint32_t getPoolGetWorkCount() {
	return s_poolGetWorkCount;
}

// target = 2 ^ 256 / difficulty
void computeTarget(mpz_t mpz_difficulty, mpz_t &mpz_target) {
	mpz_t mpz_numerator;
	mpz_maxBest(mpz_numerator);
	mpz_init_set_str(mpz_target, "0", 10);
	mpz_div(mpz_target, mpz_numerator, mpz_difficulty);
}

// difficulty = 2 ^ 256 / target
void computeDifficulty(mpz_t mpz_target, mpz_t &mpz_difficulty) {
	mpz_t mpz_numerator;
	mpz_maxBest(mpz_numerator);
	mpz_init_set_str(mpz_difficulty, "0", 10);
	mpz_div(mpz_difficulty, mpz_numerator, mpz_target);
}

http_connection_handle_t getHandle(const std::string &url) {
	if (s_httpHandles.find(url) == s_httpHandles.end()) {
		s_httpHandles.insert(std::make_pair(url, newHttpConnectionHandle()));
#ifdef _DEBUG
		printf("\n===> new http handle for %s\n\n", url.c_str());
#endif
	}
	return s_httpHandles[url];
}

static bool performGetWorkRequest(const std::string &nodeUrl, std::string &response) 
{
	char getWorkParams[512];
	snprintf(
		getWorkParams, 
		sizeof(getWorkParams), 
		"{\"jsonrpc\":\"2.0\", \"id\" : %d, \"method\" : \"aqua_getWork\", \"params\" : null}",
		s_nodeReqId++);	
	return httpPost(getHandle(nodeUrl), nodeUrl, getWorkParams, response, &HTTP_HEADER);
}

typedef struct {
	std::string difficulty;
	std::string target;
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
			"%-16s : %s\n"
			"%-16s : %s\n"
			"%-16s : %s\n"
			"%-16s : %d",
			"height", b.height.c_str(),
			"diff", b.difficulty.c_str(),
			"target", b.target.c_str(),
			"version", b.version);
	}
	else {
		snprintf(buf, sizeof(buf), 
			"%-16s : %s\n"
			"%-16s : %s\n"
			"%-16s : %s\n"
			"%-16s : %s\n"
			"%-16s : %s\n"
			"%-16s : %d",
			"height", b.height.c_str(),
			"miner", b.miner.c_str(),
			"diff", b.difficulty.c_str(),
			"target", b.target.c_str(),
			"nonce", b.nonce.c_str(),
			"version", b.version);
	}

	return buf;
}

static bool getBlocksInfo(const std::string &nodeUrl, t_blocksInfo &result)
{
	
	if (false) {
		printf("\n\nnot getting new block 1\n\n");
		return false;
	}
	
	
	if (nodeUrl.size() == 0) {
		printf("\n\nnot getting new block 2\n\n");
		return false;
	}

	//printf("getting new block info\n");
	//printf("node url: %s\n", nodeUrl.c_str());
	
	auto getBlockJson = [&nodeUrl](std::string blockNum, t_blockInfo &res) -> bool {
		char getPendingBlockParams[512];
		snprintf(
			getPendingBlockParams,
			sizeof(getPendingBlockParams),
			"{\"jsonrpc\":\"2.0\", \"id\" : %d, \"method\" : \"aqua_getBlockByNumber\", \"params\" : [\"%s\", false]}",
			s_nodeReqId++,
			blockNum.c_str());
		
		std::string resp;
		if (!httpPost(getHandle(nodeUrl), nodeUrl, getPendingBlockParams, resp, &HTTP_HEADER))
			return false;

		std::vector<std::string> params;

		Document doc;
		doc.Parse(resp.c_str());
		if (!doc.HasMember(RESULT) || !doc[RESULT].IsObject())
			return false;

		auto result = doc[RESULT].GetObject();

		// read difficulty
		const char* DIFFICULTY = "difficulty";
		if (!result.HasMember(DIFFICULTY)) {
			return false;
		}
		mpz_t mpz_difficulty;
		decodeHex(result[DIFFICULTY].GetString(), mpz_difficulty);
		res.difficulty = mpzToString(mpz_difficulty);

		// compute target from difficulty
		mpz_t mpz_target;
		computeTarget(mpz_difficulty, mpz_target);
		res.target = mpzToString(mpz_target);

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

static bool setCurrentWork(const Document &work, WorkParams &workParams) {
	// result[0], 32 bytes hex encoded current block header pow-hash
	// result[1], Hash Version. Previously 32 bytes hex encoded seed hash used for DAG
	// result[2], 32 bytes hex encoded boundary condition ("target"), 2^256/difficulty
	if (!work.HasMember(RESULT))
		return false;
	std::vector<std::string> resultArray;
	auto arr = work[RESULT].GetArray();
	for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
		resultArray.emplace_back(arr[i].GetString());
	}

	char buf[256];

	// hash version
	switch (resultArray[1][65]) {
		case '2':
			workParams.version = 2;
			break;
		case '3':
			workParams.version = 3;
			break;
		case '4':
			workParams.version = 4;
			break;
		default:
			workParams.version = -1;
			break;
	}
	//printf("Hash: %s\n", resultArray[0].c_str());
	//printf("Version: %c\n", resultArray[1][65]);
	//printf("Difficulty: %s\n", resultArray[2].c_str());
	
	// compute target
	if (workParams.hash == resultArray[0]) {
		// printf("not setting new existing work\n");
		return true;
	}
	// printf("setting new work\n");
	workParams.target = resultArray[2];
	decodeHex(workParams.target.c_str(), workParams.mpz_target);
	gmp_snprintf(buf, sizeof(buf), "%Zd", workParams.mpz_target);
	workParams.target.assign(buf);

	// compute difficulty
	mpz_t mpz_difficulty;
	computeDifficulty(workParams.mpz_target, mpz_difficulty);
	gmp_snprintf(buf, sizeof(buf), "%Zd", mpz_difficulty);
	workParams.difficulty.assign(buf);

	// store work hash
	workParams.hash = resultArray[0];

	return true;
}

bool requestPoolParams(const MiningConfig& config, WorkParams &workParams, bool verbose)
{	
	// get work
	std::string getWorkResponse;
	bool postRequestOk = performGetWorkRequest(config.getWorkUrl, getWorkResponse);
	if (!postRequestOk) {
		if (verbose)
			logLine(UPDATE_THREAD_LOG_PREFIX, "Pool not responding (%s)", config.getWorkUrl.c_str());
		return false;
	}

	// parse work json
	Document work;
	work.Parse(getWorkResponse.c_str());
	if (!work.IsObject()) {
		if (verbose) {
			logLine(UPDATE_THREAD_LOG_PREFIX, "Cannot get work params from pool (%s)", config.getWorkUrl.c_str());
		}
		return false;
	}

	// update current work params with the new work
	if (!setCurrentWork(work, workParams)) {
		if (verbose)
			logLine(UPDATE_THREAD_LOG_PREFIX, "Error parsing pool work params (%s)\n%s\n", config.getWorkUrl.c_str(), getWorkResponse.c_str());
		return false;
	}

	return true;
}

WorkParams currentWorkParams() {
	WorkParams ret;
	s_workParams_mutex.lock();
	{
		ret = s_workParams;
	}
	s_workParams_mutex.unlock();
	return ret;
}

// regularly polls the pool to get new WorkParams when block changes
void updateThreadFn() {
	auto tStart = high_resolution_clock::now();
	bool solo = miningConfig().soloMine;

	while (s_bUpdateThreadRun) {
		WorkParams newWork;
		newWork.hash = s_workParams.hash;

		auto tNow = high_resolution_clock::now();
		std::chrono::duration<float> durationSinceLast = tNow - tStart;
		bool recomputeHashRate = false;

		// call aqua_getWork on node / pool
		// printf("\n\ngetWork\n\n");
		bool ok = requestPoolParams(miningConfig(), newWork, true);
		if (!ok) {
			const auto POOL_ERROR_WAIT_N_SECONDS = 30;
			logLine(UPDATE_THREAD_LOG_PREFIX, "problem getting new work, retrying in %ds",
				POOL_ERROR_WAIT_N_SECONDS);

			std::this_thread::sleep_for(std::chrono::seconds(POOL_ERROR_WAIT_N_SECONDS));
		}
		else {
			// we have one more successfull getWork request
			if (!solo) {
				s_poolGetWorkCount++;
			}
			// we have new work (a new block)
			if (s_workParams.hash != newWork.hash) {
				// update miner params, must be done first, as quick as possible
				s_workParams_mutex.lock();
				{
					s_workParams = newWork;
				}
				s_workParams_mutex.unlock();

				// refresh latest/pending blocks info
				auto cfg = miningConfig();
				bool hasFullNode = cfg.fullNodeUrl.size() > 0;
				std::string queryUrl = hasFullNode ?
					cfg.fullNodeUrl :
					cfg.getWorkUrl;

				// building log message
				char header[2048] = { 0 };
				snprintf(header, sizeof(header), "\n\n- New work info -\n%-16s : %s\n%-16s : %s\n%-16s : %s\n%-16s : %d\n",
					"hash", 
					newWork.hash.c_str(),
					miningConfig().soloMine ? "block difficulty" : "share difficulty",
					newWork.difficulty.c_str(),
					miningConfig().soloMine ? "block target" : "share target",
					newWork.target.c_str(),
					"hash version",
					newWork.version);

				char body[2048] = { 0 };
				t_blocksInfo blocksInfo;
				if (!getBlocksInfo(queryUrl, blocksInfo)) {
					snprintf(body, sizeof(body), 
						"Cannot show new block information (do not panic, mining might still be ok)");
				}
				else {

					if (hasFullNode) {
						snprintf(body, sizeof(body),
							"%-16s : %s\n",
							"block height",
							blocksInfo.pending.height.c_str());

						auto n = strlen(body);
						snprintf(body + n, sizeof(body) - n,
							"\n- Latest mined block info -\n%s\n\n",
							formatBlockInfo(blocksInfo.latest).c_str());
					}
				}
				
				// log new work / block info
				logLine(UPDATE_THREAD_LOG_PREFIX, "%s\n%s", header, body);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(miningConfig().refreshRateMs));
	}

	for (auto &it : s_httpHandles) {
		destroyHttpConnectionHandle(it.second);
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
