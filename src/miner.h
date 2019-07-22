#pragma once

#include "hex_encode_utils.h"

#include <stdint.h>
#include <string>
#include <stdint.h>
#include <gmp.h>
#include <argon2.h>
#include <assert.h>

// size of the hash that argon2i / argon2id will generate
const uint32_t ARGON2_HASH_LEN = 32;

// current work
struct WorkParams {
	int version = -1;
	std::string difficulty = "";
	std::string target = "";
	std::string hash = "";
	mpz_t mpz_target;
};

void startMinerThreads(int nThreads);
void stopMinerThreads();

uint32_t getTotalHashes();
uint32_t getTotalSharesSubmitted();
uint32_t getTotalSharesAccepted();
uint32_t getTotalBlocksAccepted();
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

inline void mpz_fromBytesNoInit(uint8_t* bytes, size_t count, mpz_t mpz_result) {
	const int ORDER = 1;
	const int ENDIAN = 1;
	assert(count % 4 == 0);
	mpz_import(mpz_result, count>>2, ORDER, 4, ENDIAN, 0, bytes);
}

inline void mpz_fromBytes(uint8_t* bytes, size_t count, mpz_t mpz_result) {
	mpz_init(mpz_result); // must init before import
	mpz_fromBytesNoInit(bytes, count, mpz_result);
}

void setArgonParams(long t_cost, long m_cost, long lanes);
bool argonParamsMineable();
