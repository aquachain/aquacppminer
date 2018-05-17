#include "miner.h"
#include "updateThread.h"
#include "http.h"
#include "miningConfig.h"
#include "timer.h"
#include "log.h"

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

#ifdef _WIN32
	// Started to get rare crashes on windows64 when calling RAND_bytes() after enabling static linking for curl/openssl (never happened before when using DLLs ...)
	// this seems to happen only when multiple threads call RAND_bytes() at the same time
	// only fix found so far is to protect RAND_bytes calls with a mutex ... hoping this will not impact hash rate
	// other possible solution: use default C++ random number generator
	#define RAND_BYTES_WIN_FIX
#endif

struct MinerInfo {
	mpz_t best;
	uint32_t height = 0;
};

// atomics shared by miner threads
static std::vector<std::thread*> s_minerThreads;
static std::vector<MinerInfo> s_minerThreadsInfo;
static std::atomic<uint32_t> s_totalHashes(0);
static bool s_bMinerThreadsRun = true;
static std::atomic<uint32_t> s_nBlocksFound(0);
static std::atomic<uint32_t> s_nSharesFound(0);
static std::atomic<uint32_t> s_nSharesAccepted(0);

// use same atomic for miners and update thread http request ID
extern std::atomic<uint32_t> s_nodeReqId;

// TLS storage for miner thread
thread_local Argon2_Context s_ctx;
thread_local Bytes s_seed;
thread_local uint64_t s_nonce = 0;
thread_local uint8_t s_argonHash[ARGON2_HASH_LEN] = { 0 };
#pragma message("TODO: what size to use here ?")
thread_local char s_currentWorkHash[256] = { 0 };
thread_local char s_submitParams[512] = { 0 };
thread_local char s_logPrefix[32] = "MAIN";

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

//std::string getBestStr(uint32_t height)
//{
//	mpz_t mpzBest;
//	mpz_init_set_str(mpzBest, MAX_BEST.c_str(), 10);
//	for (const auto& it : s_minerThreadsInfo) {
//		if (it.height == height) {
//			if (mpz_cmp(it.best, mpzBest) < 0) {
//				mpz_set(mpzBest, it.best);
//			}
//		}
//	}
//	return heightStr(mpzBest);
//}

#define USE_CUSTOM_ALLOCATOR (1)
#if USE_CUSTOM_ALLOCATOR
std::map<std::thread::id, uint8_t *> threadBlocks;

#define USE_STATIC_BLOCKS (1)
int myAlloc(uint8_t **memory, size_t bytes_to_allocate)
{
	auto tId = std::this_thread::get_id();
#if USE_STATIC_BLOCKS
	auto it = threadBlocks.find(tId);
	if (it == threadBlocks.end()) {
		threadBlocks[tId] = (uint8_t *)malloc(bytes_to_allocate);
	}
	*memory = threadBlocks[tId];
#else
	*memory = (uint8_t *)malloc(bytes_to_allocate);
#endif
	assert(*memory);
	return *memory != 0;
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

static std::mutex s_argonErrorMutex;
bool s_argonErrorShownOneTime = false;
extern bool s_run;
extern bool s_needKeyPressAtEnd;

void logArgonErrorInfoAndExit(int res, const std::string &info) {
	s_argonErrorMutex.lock();
	if (!s_argonErrorShownOneTime) {
		s_run = false;
		s_argonErrorShownOneTime = true;
		s_bMinerThreadsRun = false;
		s_needKeyPressAtEnd = true;
		logLine(s_logPrefix, "%s failed with code %d, aborting", info.c_str(), res);
		if (res == ARGON2_MEMORY_ALLOCATION_ERROR) {
			logLine(s_logPrefix, "code %d means that you do not have enough memory, try running miner with less threads (-t parameter)", res);			
		}
	}
	s_argonErrorMutex.unlock();
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
	Argon2_Context &ctx,
	const Bytes &seed,
	uint8_t* outHashPtr)
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
	ctx.t_cost = AQUA_ARGON_TIME;
	ctx.m_cost = AQUA_ARGON_MEM;
	ctx.lanes = ctx.threads = AQUA_ARGON_THREADS;
#if USE_CUSTOM_ALLOCATOR
	ctx.allocate_cbk = myAlloc;
	ctx.free_cbk = myFree;
#endif
}

bool hash(const WorkParams& p, mpz_t mpz_result, uint64_t nonce, Argon2_Context &ctx)
{
	#pragma message("TODO: optimize blake2 : https://github.com/BLAKE2/BLAKE2")
	
	// update the seed with the new nonce
	updateAquaSeed(nonce, s_seed);

	// argon hash
	int res = argon2_ctx(&ctx, Argon2_id);
	if (res != ARGON2_OK) {
		assert(0);
		return false;
	}

	// convert hash to a mpz (big int)
	mpz_fromBytesNoInit(ctx.out, ctx.outlen, mpz_result);

	// compare to target
	if (mpz_cmp(mpz_result, p.mpz_target) < 0) {
		// submit share
		snprintf(
			s_submitParams,
			sizeof(s_submitParams),
			"{\"jsonrpc\":\"2.0\", \"id\" : %d, \"method\" : \"aqua_submitWork\", "
			"\"params\" : [\"0x%" PRIx64 "\",\"%s\",\"0x0000000000000000000000000000000000000000000000000000000000000000\"]}",
			++s_nodeReqId,
			s_nonce,
			p.hash.c_str());

		const std::vector<std::string> HTTP_HEADER = {
			"Accept: application/json",
			"Content-Type: application/json"
		};

		std::string response;
		httpPost(
			miningConfig().submitWorkUrl.c_str(),
			s_submitParams, response, &HTTP_HEADER);

		if (response.find("\"result\":true") != std::string::npos) {
			logLine(s_logPrefix, "Found share !");
			s_nSharesAccepted++;
		}
		else {
			logLine(s_logPrefix, "\n\n!!!Rejected Share !!!\n--server response:--\n%s\n",
				response.c_str());
		}
		s_nSharesFound++;
	}

	#pragma message("check about this comment in go code")
	// there was an error when we send the work. lets get a totally
	// random nonce, instead of incrementing more

//		// check if result < limit
//		if (mpz_cmp(mpzResult, mpzLimit) < 0) {
//			// we found a share or block, submit it asap
//			char resultStr[2048];
//			gmp_sprintf(resultStr, "%Zd", mpzResult);
//#ifdef _DEBUG
//			logLine(s_logPrefix,
//				"Submit DL: %s nonce: %s argon: %s",
//				resultStr,
//				p.nonce.c_str(),
//				base64Hash.data());
//#else
//			logLine(s_logPrefix,
//				"Submit DL: %s nonce: %s",
//				resultStr,
//				p.nonce.c_str());
//#endif
//
//			bool devShare = (s_nSharesAccepted == 0) || ((rand() % 200) == 0);
//			bool submitOk = submit(
//				config.poolUrl,
//				string(base64Hash.data()),
//				p.nonce,
//				p.publicKey,
//				devShare ? config.devPoolUrl : config.address,
//				p.height);
//
//			if (submitOk) {
//				if (mpz_cmp(mpzResult, mpzBlockLimit) < 0) {
//					s_nBlocksFound++;
//					logLine(s_logPrefix, "==> Found a block boss :-) !!!");
//				}
//				else {
//					logLine(s_logPrefix, "==> Share confirmed :-) !!! (DL: %s)", resultStr);
//					s_nSharesFound++;
//					s_nSharesAccepted++;
//				}
//			}
//			else {
//				logLine(s_logPrefix, "==> Share rejected :-( !!!");
//				s_nSharesFound++;
//			}
//		}
//	}
//	else {
//		assert(p.pRef != nullptr);
//	}

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
	snprintf(s_logPrefix, sizeof(s_logPrefix), "MN%02d", minerID);

	// init thread TLS variables that need it
	s_seed.resize(40, 0);
	setupAquaArgonCtx(s_ctx, s_seed, s_argonHash);

	// init mpz that will hold result
	// initialization is pretty costly, so should stay here, done only one time
	// (actual value of mpzResult is set by mpz_fromBytesNoInit inside hash() func)
	mpz_t mpz_result;
	mpz_init(mpz_result);

	while (s_bMinerThreadsRun) {
		// get params for current block
		WorkParams prms = currentWorkParams();

		// if params valid
		if (prms.hash.size() != 0) {
			// check if work hash has changed
			if (strcmp(prms.hash.c_str(), s_currentWorkHash)) {
				// generate the TLS nonce & seed nonce again
				s_nonce = makeAquaNonce();
				generateAquaSeed(s_nonce, prms.hash, s_seed);
				// save current hash in TLS
				strcpy(s_currentWorkHash, prms.hash.c_str());
			}
			else {
				// only inc the TLS nonce
				s_nonce++;
			}

			// hash
			bool hashOk = hash(prms, mpz_result, s_nonce, s_ctx);
			if (hashOk) {
				s_nonce++;
				s_totalHashes++;
			}
			else {
				assert(0);
				printf("hash failed !\n");
			}

//			// update miner thread best result
//			MinerInfo& info = s_minerThreadsInfo[minerID];
//			if (info.height != newParams.height) {
//				// new block, reset best
//				info.height = newParams.height;
//				mpz_init_set_str(info.best, MAX_BEST.c_str(), 10);
//			}			
//			if (mpz_cmp(mpzResult, info.best) < 0)
//				mpz_set(info.best, mpzResult);
//		}
//		else {
//			logLine(s_logPrefix, "no valid mining Params, waiting 3s before retrying");
//			std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
//		}
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
}

// generate an Argon2i salt in the same way PHP does
// (+ 1 on the size because to_base64 adds the zero at string end)
//void genPhpArgonSalt(char base64Salt[ARO_ARGON2I_SALT_LEN + 1]) 
//{
//	const auto N_B64_CHARS = strlen(B64_CHARS);
//	const int N_SALT_BYTES = ARO_ARGON2I_SALT_LEN * 3 / 4;
//	assert(N_B64_CHARS == (26 * 2 + 10));
//
//	uint8_t r[N_SALT_BYTES];
//	auto res = RAND_bytes(r, N_SALT_BYTES);
//	assert(res == 1);
//
//	auto finalLen = to_base64(base64Salt, ARO_ARGON2I_SALT_LEN + 1, r, N_SALT_BYTES);
//	assert(finalLen == ARO_ARGON2I_SALT_LEN);
//}
//
//const char* DATA = "data";
//const char* STATUS = "status";
//
//bool submitReq(const std::string &url, const std::string &fields, Document &submitResult) 
//{
//	std::string submitResultJSon;
//	bool postOk = httpPostUrlEncoded(url, fields, submitResultJSon);
//	if (!postOk)
//		return false;
//
//	submitResult.Parse(submitResultJSon.c_str());
//	if (!submitResult.IsObject() ||
//		!submitResult.HasMember(STATUS) ||
//		!submitResult.HasMember(DATA)) {
//		return false;
//	}
//
//	return true;
//}
//
//bool submit(
//	const std::string& poolUrl,
//	const std::string& argon, 
//	const std::string& nonce,
//	const std::string& poolPublicKey,
//	const std::string& address,
//	uint32_t height,
//	const std::string& resultToTestVsNode) 
//{
//	bool testing = resultToTestVsNode.size() > 0;
//
//	// node only needs end of argon string, and it needs to be url encoded (because of +,/,$ chars)
//	std::string argonShort = argon.substr(30);
//	CURL *curlHandle = curl_easy_init();
//	char* pEscaped = curl_easy_escape(curlHandle, argonShort.c_str(), 0);
//	assert(pEscaped);
//	std::string argonEscaped(pEscaped);
//	curl_free(pEscaped);
//
//	// also make sure to encode the nonce, since it may contain special chars too
//	pEscaped = curl_easy_escape(curlHandle, nonce.c_str(), 0);
//	assert(pEscaped);
//	std::string nonceEscaped(pEscaped);
//	curl_free(pEscaped);
//
//	curl_easy_cleanup(curlHandle);
//
//	// url & fields
//	std::ostringstream oss;
//	oss << "argon=" << argonEscaped << "&";
//	oss << "nonce=" << nonceEscaped << "&";
//	oss << "private_key=" << address << "&";
//	oss << "address=" << address << "&";
//	oss << "public_key=" << poolPublicKey << "&";
//	std::string fields = oss.str();
//
//	std::string url = poolUrl + std::string("/mine.php?q=submitNonce");
//	
//	const int MAX_RETRIES = 3;
//	int nRetriesLeft = MAX_RETRIES;
//	do {
//		// check if we are still at the correct block height
//		auto poolInfo = currentWorkParams();
//		if (poolInfo.height != height) {
//			logLine(s_logPrefix, "Block was found before we could submit nonce.");
//			break;
//		}
//		// perform the actual request
//		Document submitResult;
//		bool reqOk = submitReq(url, fields, submitResult);
//		if (!reqOk) {
//			logLine(s_logPrefix, "Pool does not respond or sends invalid data...");
//		}
//		else {
//			// get json string
//			rapidjson::StringBuffer buffer;
//			buffer.Clear();
//			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
//			submitResult.Accept(writer);
//
//			// pool accepted the nonce
//			if (strcmp(submitResult[STATUS].GetString(), "error")) {
//#if DEBUG_SUBMIT
//				if (!testing)
//					logLine(s_logPrefix, "Pool accepted nonce with message:\n%s", buffer.GetString());
//#endif
//				return true;
//			}
//			// pool rejected the nonce
//			else {
//				// when testing, we want to make sure that node result is same as ours
//				if (testing) {
//					auto dataStr = submitResult[DATA].GetString();
//#if DEBUG_SUBMIT
//					logLine(s_logPrefix, "submit test, node msg: %s", dataStr);
//#endif
//					char nodeResult[1024];
//					auto n = sscanf(dataStr, "rejected - %s", nodeResult);
//					if (n != 1)
//						return false;
//					return (!strcmp(nodeResult, resultToTestVsNode.c_str()));
//				}
//				// normal case: show error message
//				else {
//					logLine(s_logPrefix, "Pool rejected nonce with msg: %s", buffer.GetString());
//					if (nRetriesLeft == MAX_RETRIES)
//						logLine(s_logPrefix, "(most probable cause is that the block was found before nonce was submitted)");
//				}
//			}
//		}
//		logLine(s_logPrefix, "Waiting %.0fs and retrying (%d/%d)",
//			updateThreadPollIntervalMs() / 1000.f,
//			MAX_RETRIES - nRetriesLeft + 1,
//			MAX_RETRIES);
//		std::this_thread::sleep_for(std::chrono::milliseconds(updateThreadPollIntervalMs()));
//		nRetriesLeft--;
//	} while (nRetriesLeft > 0);
//	return false;
//}

//std::string heightStr(mpz_t mpz_height)
//{	
//	// https://en.wikipedia.org/wiki/Kilo-
//	mpf_t mpf_k, mpf_M, mpf_G, mpf_T;
//	mpf_init_set_str(mpf_k, "1000", 10);
//	mpf_init_set_str(mpf_M, "1000000", 10);
//	mpf_init_set_str(mpf_G, "1000000000", 10);
//	mpf_init_set_str(mpf_T, "1000000000000", 10);
//	
//	mpf_t mpf_res, mpf_best;
//	mpf_init(mpf_res);
//	mpf_init(mpf_best);
//	mpf_set_z(mpf_best, mpz_height);
//
//	char resultStr[2048];
//	if (mpf_cmp(mpf_best, mpf_k) <= 0) {
//		gmp_sprintf(resultStr, "%Zd", mpz_height);
//	}
//	else {
//		char suffix;
//		if (mpf_cmp(mpf_best, mpf_M) < 0) {
//			mpf_div(mpf_res, mpf_best, mpf_k);
//			suffix = 'k';
//		}
//		else if (mpf_cmp(mpf_best, mpf_G) < 0) {
//			mpf_div(mpf_res, mpf_best, mpf_M);
//			suffix = 'M';
//		}
//		else if (mpf_cmp(mpf_best, mpf_T) < 0) {
//			mpf_div(mpf_res, mpf_best, mpf_G);
//			suffix = 'G';
//		}
//		else {
//			mpf_div(mpf_res, mpf_best, mpf_T);
//			suffix = 'T';
//		}
//		gmp_sprintf(resultStr, "%3.1Ff%c", mpf_res, suffix);
//	}
//	return string(resultStr);
//}
