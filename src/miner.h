#pragma once

#include "hex_encode_utils.h"

#include <stdint.h>
#include <string>
#include <stdint.h>
#include <gmp.h>
#include <argon2.h>

// argon2 params for aquachain
const int AQUA_ARGON_MEM = 1;
const int AQUA_ARGON_THREADS = 1;
const int AQUA_ARGON_TIME = 1;

// size of the hash that argon2i / argon2id will generate
const uint32_t ARGON2_HASH_LEN = 32;

// current work
struct WorkParams {
	std::string difficulty = "";
	std::string target = "";
	std::string hash = "";
};

bool hash(const WorkParams& p, mpz_t result, uint64_t nonce);
void startMinerThreads(int nThreads);
void stopMinerThreads();

uint32_t getTotalHashes();
//std::string getBestStr(uint32_t height);
uint32_t getTotalSharesSubmitted();
uint32_t getTotalSharesAccepted();
uint32_t getTotalBlocksAccepted();

//bool submit(
//	const std::string& poolUrl,
//	const std::string& argon,
//	const std::string& nonce,
//	const std::string& poolPublicKey,
//	const std::string& address,
//	uint32_t height,
//	const std::string& resultToTestVsNode = "");

void freeCurrentThreadMiningMemory();

void mpz_maxBest(mpz_t mpz_n);

bool generateAquaSeed(
	uint64_t nonce,
	std::string workHashHex,
	Bytes& seed);

void setupAquaArgonCtx(
	Argon2_Context &ctx,
	const Bytes &seed,
	uint8_t* outHashPtr);
