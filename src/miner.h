#pragma once

#include <stdint.h>

#include <gmp.h>

#include <string>
#include <stdint.h>

#include "tests.h"

// constants
const uint32_t ARO_ARGON2I_ITERATIONS = 1;
const uint32_t ARO_ARGON2I_MEMORY = 524288;
const uint32_t ARO_ARGON2I_PARALLELISM = 1;
const uint32_t ARO_ARGON2I_SALT_LEN = 16;
const uint32_t ARO_ARGON2I_HASH_LEN = 32;

extern const std::string MAX_BEST;

struct HashParams {
	std::string nonce = "";
	std::string publicKey = "";
	std::string block = "";
	std::string difficulty = "";
	uint32_t limit = 0;
	uint32_t height = 0;
	const HashReference* pRef = nullptr;
	bool compareResultToNode = false;
};

struct HashTimings {
	float argonTime = 0.f;
	float totalTime = 0.f;
};

bool hash(const HashParams& p, mpz_t result, HashTimings* pTimings = nullptr);
void startMinerThreads(int nThreads);
void stopMinerThreads();

uint32_t getTotalHashes();
std::string getBestStr(uint32_t height);
uint32_t getTotalSharesSubmitted();
uint32_t getTotalSharesAccepted();
uint32_t getTotalBlocksAccepted();

bool submit(
	const std::string& poolUrl,
	const std::string& argon,
	const std::string& nonce,
	const std::string& poolPublicKey,
	const std::string& address,
	uint32_t height,
	const std::string& resultToTestVsNode = "");

void makeNonce(std::string &nonceStr);
std::string heightStr(mpz_t mpz_height);

void freeCurrentThreadMiningMemory();
