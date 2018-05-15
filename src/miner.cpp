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

static std::vector<std::thread*> s_minerThreads;
static std::vector<MinerInfo> s_minerThreadsInfo;
static std::atomic<uint32_t> s_totalHashes(0);
static bool s_bMinerThreadsRun = true;
static std::atomic<uint32_t> s_nBlocksFound(0);
static std::atomic<uint32_t> s_nSharesFound(0);
static std::atomic<uint32_t> s_nSharesAccepted(0);

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

#define USE_CUSTOM_ALLOCATOR (0)
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

bool hash(const HashParams& p, mpz_t mpzResult, HashTimings* pTimings)
{
//	assert(p.nonce.size() > 0);
//	assert(p.publicKey.size() > 0);
//	assert(p.block.size() > 0);
//	assert(p.difficulty.size() > 0);
//
//	// get global parameters
//	auto config = miningConfig();
//
//	// set result to 0 by default
//	mpz_init(mpzResult);
//
//	// init timing stuff
//	Timer tTotal, tArgon;
//	bool doTiming = pTimings != nullptr;
//	if (doTiming)
//		tTotal.start();
//
//	// construct base pwd
//	string pwd;
//	pwd.reserve(1024);
//	pwd.append(p.publicKey).append("-").append(p.nonce).append("-").append(p.block).append("-").append(p.difficulty);
//	assert(pwd.size() <= 1024);
//
//	// generate a salt (raw bytes) for Argon2i
//	uint8_t salt[ARO_ARGON2I_SALT_LEN + 1];
//#ifdef _DEBUG
//	memset(salt, 0, sizeof(salt));
//#endif
//	if (p.pRef) {
//		// testing, use provided salt
//		string salt64 = p.pRef->getSaltBase64();
//		size_t saltLen = salt64.size();
//		from_base64(salt, &saltLen, salt64.c_str());
//		assert(saltLen == ARO_ARGON2I_SALT_LEN);
//	}
//	else {
//		// real mining, generate salt like PHP does
//		genPhpArgonSalt((char*)salt);
//	}
//	for (int i = 0; i < ARO_ARGON2I_SALT_LEN; i++) {
//		assert(salt[i] != 0);
//	}
//
//	// buffers that will hold the raw and encoded hashes
//	uint8_t rawHash[ARO_ARGON2I_HASH_LEN];
//	size_t base64HashLen = argon2_encodedlen(ARO_ARGON2I_ITERATIONS, ARO_ARGON2I_MEMORY, ARO_ARGON2I_PARALLELISM, ARO_ARGON2I_SALT_LEN, ARO_ARGON2I_HASH_LEN, Argon2_i);
//	std::vector<char> base64Hash(base64HashLen);
//
//	// setup argon2i
//	Argon2_Context ctx;
//	memset(&ctx, 0, sizeof(Argon2_Context));
//	ctx.out = rawHash;
//	ctx.outlen = ARO_ARGON2I_HASH_LEN;
//	ctx.pwd = (uint8_t*)pwd.c_str();
//	ctx.pwdlen = (uint32_t)pwd.size();
//	ctx.salt = salt;
//	ctx.saltlen = ARO_ARGON2I_SALT_LEN;
//	ctx.version = ARGON2_VERSION_NUMBER;
//	ctx.flags = ARGON2_DEFAULT_FLAGS;
//
//	ctx.t_cost = ARO_ARGON2I_ITERATIONS;
//	ctx.m_cost = ARO_ARGON2I_MEMORY;
//	// cf argon2.c L75:
//	//		if (instance.threads > instance.lanes)
//	//			instance.threads = instance.lanes;
//	// so not usefull trying to set different values for lanes & threads
//	ctx.lanes = ARO_ARGON2I_PARALLELISM;
//	ctx.threads = ARO_ARGON2I_PARALLELISM;
//
//#if USE_CUSTOM_ALLOCATOR
//	ctx.allocate_cbk = myAlloc;
//	ctx.free_cbk = myFree;
//#endif
//
//	// hash the password with argon2i and encode it to base64
//	if (doTiming)
//		tArgon.start();
//	{
//		int res = argon2_ctx(&ctx, Argon2_i);
//		if (res != ARGON2_OK) {
//			logArgonErrorInfoAndExit(res, "argon2_ctx");
//			return false;
//		}
//		res = encode_string(base64Hash.data(), base64HashLen, &ctx, Argon2_i);
//		if (res != ARGON2_OK) {
//			logArgonErrorInfoAndExit(res, "encode_string");
//			return false;
//		}
//	}
//	if (doTiming)
//		tArgon.end(pTimings->argonTime);
//
//	// concatenate base & pwdHash
//	char toHash[4096];
//	strcpy(toHash, pwd.c_str());
//	strcat(toHash, base64Hash.data());
//
//	// apply 6 rounds of SHA512 via OpenSSL
//	unsigned char curHash[SHA512_DIGEST_LENGTH];
//	SHA512((unsigned char*)toHash, strlen(toHash), (unsigned char*)&curHash);
//	for (int i = 0; i < 5; i++) {
//		unsigned char newHash[SHA512_DIGEST_LENGTH];
//		SHA512((unsigned char*)curHash, SHA512_DIGEST_LENGTH, (unsigned char*)newHash);
//		std::memcpy(curHash, newHash, SHA512_DIGEST_LENGTH);
//	}
//
//	// extract 8 specific bytes from hash to build duration string
//	char durationStr[1024];
//	size_t indices[8] = { 10, 15, 20, 23, 31, 40, 45, 55 };
//	char *pS = durationStr;
//	for (int i = 0; i < sizeof(indices) / sizeof(indices[0]); i++) {
//		pS += sprintf(pS, "%d", curHash[indices[i]]);
//	}
//	*pS = 0;
//
//	// trim duration string leading zeros
//	char *durationStrFinal = durationStr;
//	while (*durationStrFinal == '0')
//		durationStrFinal++;
//
//	// check for failure if asked
//	if (p.pRef) {
//		if (p.pRef->argon2i != string(base64Hash.data())) {
//			return false;
//		}
//		char hashHex[SHA512_DIGEST_LENGTH * 2 + 1];
//		memset(hashHex, 0, sizeof(hashHex));
//		char* pS = hashHex;
//		for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
//			pS += sprintf(pS, "%02x", curHash[i]);
//		}
//		*pS = 0;
//		if (p.pRef->finalHash != string(hashHex)) {
//			return false;
//		}
//		if (p.pRef->duration != string(durationStrFinal)) {
//			return false;
//		}
//	}
//
//	// divide duration by difficulty to get final quotient string
//	mpz_t mpzDiff, mpzDuration;
//	mpz_init_set_str(mpzDiff, p.difficulty.c_str(), 10);
//	mpz_init_set_str(mpzDuration, durationStrFinal, 10);
//	mpz_tdiv_q(mpzResult, mpzDuration, mpzDiff);
//	
//	if (p.limit > 0) {
//		mpz_t mpzLimit, mpzBlockLimit;
//		mpz_init_set_ui(mpzLimit, p.limit);
//		mpz_init_set_ui(mpzBlockLimit, (uint32_t)240);
//
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
//
//	// handle tests
//	if (p.pRef || p.compareResultToNode) {
//		char resultStr[2048];
//		gmp_sprintf(resultStr, "%Zd", mpzResult);
//
//		// user provided reference values
//		if (p.pRef) {
//			if (p.pRef->result != string(resultStr)) {
//				return false;
//			}
//		}
//
//		// user asked to compare result to node (this is for testing)
//		if (p.compareResultToNode) {
//			bool ok = submit(
//				config.poolUrl,
//				string(base64Hash.data()),
//				p.nonce,
//				p.publicKey,
//				config.address,
//				p.height,
//				resultStr);
//			if (!ok)
//				return false;
//		}
//	}
//
//	// finalize timings
//	if (doTiming)
//		tTotal.end(pTimings->totalTime);
	return true;
}

void makeNonce(std::string &nonceStr) 
{
	//// default php miners has a variable size (~[41-43])
	//const size_t NONCE_N_CHARS = 42;

	//uint8_t r[NONCE_N_CHARS];
	//auto res = RAND_bytes(r, NONCE_N_CHARS);
	//assert(res == 1);

	//char nonce[NONCE_N_CHARS + 1];
	//for (int i = 0; i < NONCE_N_CHARS; i++) {
	//	nonce[i] = B64_CHARS[r[i] % 62];
	//}
	//nonce[NONCE_N_CHARS] = 0;

	//nonceStr.assign(nonce);
}

void minerThreadFn(int minerID) 
{
	snprintf(s_logPrefix, sizeof(s_logPrefix), "MN%02d", minerID);

	while (s_bMinerThreadsRun) {
		// get params for current block
		HashParams newParams = currentHashParams();

		// if params valid
		if (newParams.blockHeaderHash.size() != 0) {
			// sleep for now
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			
			s_totalHashes++;

//			// generate a nonce
//			assert(newParams.nonce.size() == 0);
//#if USE_UNIQUE_NONCE
//			newParams.nonce = uniqueNonce;
//#else
//			makeNonce(newParams.nonce);
//#endif
//#ifdef TEST_CRASH_RAND_BYTES
//			if ((rand() % 100000) == 0) {
//				printf("dummy text1 => %s\n", newParams.nonce.c_str());
//			}
//			uint8_t salt[ARO_ARGON2I_SALT_LEN + 1];
//			genPhpArgonSalt((char*)salt);
//			if ((rand() % 100000) == 0) {
//				printf("dummy text2 => %d\n", salt[0]);
//			}
//			continue;
//#endif
//			// hash
//			mpz_t mpzResult;
//			hash(newParams, mpzResult, nullptr);
//			s_totalHashes++;
//#if DEBUG_MINER
//			logLine(s_logPrefix, "%s %16s/%9s (height %u)",
//				result.c_str(), 
//				newParams.difficulty.c_str(),
//				newParams.height);
//#endif
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
//		
//		// handle relax mode
//		if (relaxMode) {
//			auto nowT = high_resolution_clock::now();
//			auto elapsed = nowT - lastPauseT;
//			if (elapsed.count() > RELAX_MODE_SLEEP_INTERVAL_SECONDS) {
//				std::this_thread::sleep_for(std::chrono::milliseconds(RELAX_MODE_SLEEP_DURATION_MS));
//			}
//			lastPauseT = high_resolution_clock::now();
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
//		auto poolInfo = currentHashParams();
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
