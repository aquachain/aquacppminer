#include "miner.h"
#include "updateThread.h"
#include "http.h"
#include "miningConfig.h"
#include "timer.h"
#include "log.h"
#include "args.h"

#include <openssl/ssl.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <assert.h>
#include <thread>
#include <map>
#include <atomic>
#include <sstream>
#include <vector>
#include <mutex>
#include <chrono>

#include <argon2.h>

using namespace rapidjson;
using std::chrono::high_resolution_clock;
using std::string;

#define DEBUG_NONCES (1)

#ifdef _WIN32
	// Started to get rare crashes on windows64 when calling RAND_bytes() after enabling static linking for curl/openssl (never happened before when using DLLs ...)
	// this seems to happen only when multiple threads call RAND_bytes() at the same time
	// only fix found so far is to protect RAND_bytes calls with a mutex ... hoping this will not impact hash rate
	// other possible solution: use default C++ random number generator
	#define RAND_BYTES_WIN_FIX
#endif

struct MinerInfo {
	MinerInfo() :
		needRegenSeed(false)
	{
	}

	MinerInfo(const MinerInfo& origin) :
		needRegenSeed(false)
	{
		logPrefix = origin.logPrefix;
	}

	std::atomic<bool> needRegenSeed;
	std::string logPrefix;
};

// atomics shared by miner threads
static std::vector<std::thread*> s_minerThreads;
static std::vector<MinerInfo> s_minerThreadsInfo;
static std::atomic<uint32_t> s_totalHashes(0);
static bool s_bMinerThreadsRun = true;
static std::atomic<uint32_t> s_nBlocksFound(0);
static std::atomic<uint32_t> s_nSharesFound(0);
static std::atomic<uint32_t> s_nSharesAccepted(0);
extern std::atomic<int> s_version;

// use same atomic for miners and update thread http request ID
extern std::atomic<uint32_t> s_nodeReqId;

// TLS storage for miner thread
thread_local Argon2_Context s_ctx;
thread_local Bytes s_seed;
thread_local uint64_t s_nonce = 0;
thread_local uint8_t s_argonHash[ARGON2_HASH_LEN] = { 0 };
thread_local int s_minerThreadID = { -1 };
thread_local char s_currentWorkHash[256] = { 0 };
thread_local char s_logPrefix[32] = "MINE";
thread_local uint64_t s_threadHashes = 0;
thread_local uint64_t s_threadShares = 0;

// need to be able to stop main loop from miner threads
extern bool s_run;

// argon2id params for aquachain each HF
const std::vector<int> AQUA_HF7 = { 1, 1, 1 };
const std::vector<int> AQUA_HF8 = { 1, 16, 1 };
const std::vector<int> AQUA_HF9 = { 1, 32, 1 };
const std::vector<int> AQUA_HF10 = { 1, 64, 1 };

const std::vector<int> memorycosts = { 0, AQUA_HF7[1], AQUA_HF8[1], AQUA_HF9[1], AQUA_HF10[1]};

uint32_t version2memcost(int version) {
  switch (version) {
    case 2:
      return 1;
    case 3:
      return 16;
    case 4:
      return 32;
    case 5: 
      return 64;
  }
  return -1;
}

void mpz_maxBest(mpz_t mpz_n) {
	mpz_t mpz_two, mpz_exponent;
	mpz_init_set_str(mpz_two, "2", 10);
	mpz_init_set_str(mpz_exponent, "256", 10);
	mpz_init_set_str(mpz_n, "0", 10);
	mpz_pow_ui(mpz_n, mpz_two, mpz_get_ui(mpz_exponent));
}

#ifdef RAND_BYTES_WIN_FIX
int threadSafe_RAND_bytes(unsigned char *buf, int num) {
#if 1
	static std::mutex s_nonceGen_mutex;
	s_nonceGen_mutex.lock();
	int res = RAND_bytes(buf, num);
	s_nonceGen_mutex.unlock();
	return res;
#else
	for (int i = 0; i < num; i++)
		buf[i] = rand() & 0xFF;
	return 1;
#endif
}
#define RAND_bytes threadSafe_RAND_bytes
#endif

uint32_t getTotalSharesSubmitted()
{
	return s_nSharesFound;
}

uint32_t getTotalSharesAccepted()
{
	return s_nSharesAccepted;
}

uint32_t getTotalHashes()
{
	return s_totalHashes;
}

uint32_t getTotalBlocksAccepted()
{
	return s_nBlocksFound;
}

#define USE_CUSTOM_ALLOCATOR (0)
#if USE_CUSTOM_ALLOCATOR
static std::mutex s_alloc_mutex;
std::map<std::thread::id, uint8_t*> threadBlocks;

#define USE_STATIC_BLOCKS (0)
int myAlloc(uint8_t **memory, size_t bytes_to_allocate)
{
	auto tId = std::this_thread::get_id();
#if USE_STATIC_BLOCKS
	auto it = threadBlocks.find(tId);
	if (it == threadBlocks.end()) {
		s_alloc_mutex.lock();
		{
			threadBlocks[tId] = (uint8_t *)malloc(bytes_to_allocate);
		}
		*memory = threadBlocks[tId];
		s_alloc_mutex.unlock();
	}
	else {
		*memory = threadBlocks[tId];
	}
#else
	*memory = (uint8_t *)malloc(bytes_to_allocate);
#endif
	assert(*memory);
	return *memory != nullptr;
}

void myFree(uint8_t *memory, size_t bytes_to_allocate)
{
#if !USE_STATIC_BLOCKS
	if (memory)
		free(memory);
#endif
}
#endif

void freeCurrentThreadMiningMemory() {
#if USE_CUSTOM_ALLOCATOR
	auto tId = std::this_thread::get_id();
	auto it = threadBlocks.find(tId);
	if (it != threadBlocks.end()) {
		free(threadBlocks[tId]);
		threadBlocks.erase(it);
	}
#endif
}

bool generateAquaSeed(
	uint64_t nonce,
	std::string workHashHex,
	Bytes& seed)
{
	const size_t AQUA_NONCE_OFFSET = 32;

	auto hashBytesRes = hexToBytes(workHashHex);
	if (!hashBytesRes.first) {
		seed.clear();
		assert(0);
		return false;
	}
	if (hashBytesRes.second.size() != AQUA_NONCE_OFFSET) {
		seed.clear();
		assert(0);
		return false;
	}

	seed = hashBytesRes.second;
	for (int i = 0; i < 8; i++) {
		seed.push_back(byte(nonce >> (i * 8)) & 0xFF);
	}

	return true;
}

void updateAquaSeed(
	uint64_t nonce,
	Bytes& seed)
{
	const size_t AQUA_NONCE_OFFSET = 32;
	for (int i = 0; i < 8; i++) {
		seed[AQUA_NONCE_OFFSET + i] = byte(nonce >> (i * 8)) & 0xFF;
	}
}


void setupAquaArgonCtx(
	Argon2_Context &ctx, // params
	const Bytes &seed,   // input
	uint8_t* outHashPtr) // output
{
	memset(&ctx, 0, sizeof(Argon2_Context));
	ctx.out = outHashPtr;
	ctx.outlen = ARGON2_HASH_LEN;
	ctx.pwd = const_cast<uint8_t*>(seed.data());
	assert(seed.size() == 40);
	ctx.pwdlen = 40;
	ctx.salt = NULL;
	ctx.saltlen = 0;
	ctx.version = ARGON2_VERSION_NUMBER;
	ctx.flags = ARGON2_DEFAULT_FLAGS;
	ctx.t_cost = 1;
	ctx.m_cost = version2memcost(s_version);
	ctx.lanes = 1;
	ctx.threads = 1;
#if USE_CUSTOM_ALLOCATOR
	printf("using custom allocator\n");
	ctx.allocate_cbk = myAlloc;
	ctx.free_cbk = myFree;
#endif
}

std::string nonceToString(uint64_t nonce) {
	// raw hex value
	char tmp[256];
	snprintf(tmp, sizeof(tmp),"%" PRIx64, nonce);
	// pad with zeroes
	std::string res(tmp);
	while (res.size() < 16) {
		res = std::string("0") + res;
	}
	// must start with 0x
	res = std::string("0x") + res;
	return res;
}

static std::mutex s_submit_mutex;
static http_connection_handle_t s_httpHandleSubmit = nullptr;

void submitThreadFn(uint64_t nonceVal, std::string hashStr, int minerThreadId)
{
	const std::vector<std::string> HTTP_HEADER = {
		"Accept: application/json",
		"Content-Type: application/json"
	};

	MinerInfo* pMinerInfo = &s_minerThreadsInfo[minerThreadId];
	auto nonceStr = nonceToString(nonceVal);
	char submitParams[512] = { 0 };

	snprintf(
		submitParams,
		sizeof(submitParams),
		"{\"jsonrpc\":\"2.0\", \"id\" : %d, \"method\" : \"aqua_submitWork\", "
		"\"params\" : [\"%s\",\"%s\",\"0x0000000000000000000000000000000000000000000000000000000000000000\"]}",
		1,
		nonceStr.c_str(),
		hashStr.c_str());

	std::string response;
	bool ok = false;
	printf("[submit] %s\n", submitParams);

	// all submits are done through the same CURL HTTPP connection
	// so protected with a mutex
	// means that submits will be done sequentially and not in parallel
	s_submit_mutex.lock();
	{
		if (!s_httpHandleSubmit) {
			s_httpHandleSubmit = newHttpConnectionHandle();
		}
		ok = httpPost(
			s_httpHandleSubmit,
			miningConfig().submitWorkUrl.c_str(),
			submitParams, response, &HTTP_HEADER);
	}
	s_submit_mutex.unlock();

	if (!ok) {
		logLine(
			pMinerInfo->logPrefix,
			"\n\n!!! httpPost failed while trying to submit nonce %s!!!\n",
			nonceStr.c_str());
			std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	}
	else {
		// check that "result" is true
		const char* RESULT = "result";
		Document doc;
		doc.Parse(response.c_str());
		bool accepted = false;
		if (doc.IsObject() && doc.HasMember(RESULT)) {
			if (doc[RESULT].IsString()) {
				accepted = !strcmp(doc[RESULT].GetString(), "true");
			}
			else if (doc[RESULT].IsBool()) {
				accepted = doc[RESULT].GetBool();
			}
		}

		// log
		if (accepted) {
			logLine(
				pMinerInfo->logPrefix, "%s, nonce = %s",
				miningConfig().soloMine ? "Found block !" : "Found share !",
				nonceStr.c_str()
			);
			s_nSharesAccepted++;
		}
		else {
			logLine(
				pMinerInfo->logPrefix,
				"\n\n!!! Rejected %s, nonce = %s!!!\n--server response:--\n%s\n",
				miningConfig().soloMine ? "block" : "share",
				nonceStr.c_str(),
				response.c_str());
			pMinerInfo->needRegenSeed = true;
		}
	}
	s_nSharesFound++;
}

bool aquahash(const int version, Argon2_Context *ctx){
    ctx->m_cost = version2memcost(version);
 //   printf("mcost=%d\n", ctx->m_cost);
	int res = argon2_ctx(ctx, Argon2_id);
	if (res != ARGON2_OK) {
		logLine(s_logPrefix, "Error: argon2 failed with code %d", res);
		assert(0);
		return false;
	}
    return true;
}

bool hash(const WorkParams& p, mpz_t mpz_result, uint64_t nonce, Argon2_Context &ctx)
{
    if (p.version < 2) {
      printf("invalid algorithm version\n");
      return false;
    }

	// update the seed with the new nonce
	updateAquaSeed(nonce, s_seed);

	// argon hash
    int res = aquahash(s_version, &ctx);

	// convert hash to a mpz (big int)
	mpz_fromBytesNoInit(ctx.out, ctx.outlen, mpz_result);

	// compare to target
	bool needSubmit = mpz_cmp(mpz_result, p.mpz_target) < 0;
	if (needSubmit) {
		if (miningConfig().soloMine) {
			// for solo mining we do a synchronous submit ASAP
			submitThreadFn(s_nonce, p.hash, s_minerThreadID);
		}
		else {
			// for pool mining we launch a thread to submit work asynchronously
			// like that we can continue mining while curl performs the request & wait for a response
			std::thread{ submitThreadFn, s_nonce, p.hash, s_minerThreadID}.detach();

			// sleep for a short duration, to allow the submit thread launch its request asap
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	s_threadShares++;
	return true;
}

uint64_t makeAquaNonce()
{
	uint64_t nonce = 0;
	auto ok = RAND_bytes((uint8_t*)&nonce, sizeof(nonce));
	assert(ok == 1);
	return nonce;
}

void minerThreadFn(int minerID)
{
	// record thread id in TLS
	s_minerThreadID = minerID;

	// generate log prefix
	snprintf(s_logPrefix, sizeof(s_logPrefix), "MN%02d", minerID);
	s_minerThreadsInfo[minerID].logPrefix.assign(s_logPrefix);

	// init thread TLS variables that need it
	s_seed.resize(40, 0);
	setupAquaArgonCtx(s_ctx, s_seed, s_argonHash);

	// init mpz that will hold result
	// initialization is pretty costly, so should stay here, done only one time
	// (actual value of mpzResult is set by mpz_fromBytesNoInit inside hash() func)
	mpz_t mpz_result;
	mpz_init(mpz_result);

	bool solo = miningConfig().soloMine;
	int version = -1;


	while (s_bMinerThreadsRun) {
		// get params for current block
		WorkParams prms = currentWorkParams();
		if (version2memcost(s_version) != s_ctx.m_cost) {
            if (prms.version == 0) {
              printf("hash version must be 2 or higher\n");
              break;
            }
			version = s_version;
			printf("[%s] Activating Hash Version: %d (m=%d)\n", s_logPrefix, version,version2memcost(version));
	        setupAquaArgonCtx(s_ctx, s_seed, s_argonHash);
		}
		// if params valid
		if (prms.hash.size() != 0) {
			// check if work hash has changed
			if (strcmp(prms.hash.c_str(), s_currentWorkHash)) {
				// generate the TLS nonce & seed nonce again
				s_nonce = makeAquaNonce();
				generateAquaSeed(s_nonce, prms.hash, s_seed);
				// save current hash in TLS
				strcpy(s_currentWorkHash, prms.hash.c_str());

#if DEBUG_NONCE
				logLine(s_logPrefix, "new work starting nonce: %s", nonceToString(s_nonce).c_str());
#endif
			} else if (s_minerThreadsInfo[minerID].needRegenSeed) {
					// pool has rejected the nonce, record current number of succesfull pool getWork requests
					uint32_t getWorkCountOfRejectedShare = getPoolGetWorkCount();

					// generate a new nonce
					s_nonce = makeAquaNonce();
					s_minerThreadsInfo[minerID].needRegenSeed = false;
#if DEBUG_NONCES
					logLine(s_logPrefix, "regen nonce after reject: %s", nonceToString(s_nonce).c_str());
#endif
					// wait for update thread to get new work
					//if (!solo) {
#define WAIT_NEW_WORK_AFTER_REJECT (1)
#if (WAIT_NEW_WORK_AFTER_REJECT == 0)
						logLine(s_logPrefix, "regenerated nonce after a reject, not waiting for pool to send new work !");
#else
						logLine(s_logPrefix, "Thread stopped mining because last share rejected, waiting for new work from pool");
						std::this_thread::sleep_for(std::chrono::seconds(3));
						logLine(s_logPrefix, "Thread resumes mining");
#endif
					//}
				// only inc the TLS nonce
				printf("incrementing nonce again?\n");
				s_nonce++;
				}
			

			// hash
			//if (s_nonce % 10000 == 0) {
			//	printf("Hashing m_cost=%d\n", s_ctx.m_cost);
			//}
			bool hashOk = hash(prms, mpz_result, s_nonce, s_ctx);
			if (hashOk) {
				s_nonce++;
				s_threadHashes++;
				s_totalHashes++;
			}
			else {
				assert(0);
				logLine(s_logPrefix, "FATAL ERROR: hash failed !");
				s_bMinerThreadsRun = false;
				s_run = false;
			}
		}
	}
	freeCurrentThreadMiningMemory();
}

void startMinerThreads(int nThreads)
{
	assert(nThreads > 0);
	assert(s_minerThreads.size() == 0);
	s_minerThreads.resize(nThreads);
	s_minerThreadsInfo.resize(nThreads);
	for (int i = 0; i < nThreads; i++) {
		s_minerThreads[i] = new std::thread(minerThreadFn, i);
	}
}

void stopMinerThreads()
{
	assert(s_bMinerThreadsRun);
	s_bMinerThreadsRun = false;
	for (size_t i = 0; i < s_minerThreads.size(); i++) {
		s_minerThreads[i]->join();
		delete s_minerThreads[i];
	}
	s_minerThreads.clear();
	destroyHttpConnectionHandle(s_httpHandleSubmit);
}
void setArgonParams(long t_cost, long m_cost, long lanes, Argon2_Context *ctx) {
	printf("Setting argon2 params\n");
	ctx->t_cost = t_cost;
	ctx->m_cost = m_cost;
	ctx->lanes = lanes;
}
