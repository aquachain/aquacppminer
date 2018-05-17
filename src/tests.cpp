#include "miner.h"
#include "tests.h"
#include "hex_encode_utils.h"
#include "timer.h"

#include "../phc-winner-argon2/src/core.h"

#include <assert.h>
#include <inttypes.h>

#define VERBOSE_TESTS (0)

void printBytes(std::string name, Bytes b) {
	printf("%s{", name.c_str());
	for (auto &it : b) {
		printf("%d, ", it);
	}
	printf("}\n");
}

void printBytes(std::string name, uint8_t* b, size_t count) {
	printBytes(name, { b, b + count });
}

void printBytes2(uint8_t* b, size_t count) {
	for (size_t i = 0; i < count; i++) {
		printf("%03u: %d\n", (unsigned int)i, b[i]);
	}
}

bool equal(const uint8_t* x, const uint8_t* y, size_t len) {
	for (int i = 0; i < len; i++) {
		if (x[i] != y[i]) {
			return false;
		}
	}
	return true;
}

bool testAquaHashing() {
	// reference work hash & nonce for the test
	const std::string WORK_HASH_HEX = "0xd3b5f1b47f52fdc72b1dab0b02ab352442487a1d3a43211bc4f0eb5f092403fc";
	const uint64_t NONCE = 5577006791947779410;

#if VERBOSE_TESTS
	printf("\n---- testAquaHashing() STARTS ----\n");
	mpz_t mpz_workHash;
	decodeHex(WORK_HASH_HEX.c_str(), mpz_workHash);
	auto workHashStr = mpzToString(mpz_workHash);

	printf("\n- Refs -\n");
	printf("hash  : %s\n", workHashStr.c_str());
	printf("nonce : %" PRIu64 "\n", NONCE);
#endif

	// - test seed generation
#if VERBOSE_TESTS
	printf("\n- Seed Test -\n");
#endif

	Bytes seed;
	if (!generateAquaSeed(NONCE, WORK_HASH_HEX, seed)) {
		printf("Error: generateAquaSeed failed\n");
		return false;
	}
	
	const uint8_t REF_SEED[40] = { 211, 181, 241, 180, 127, 82, 253, 199, 43, 29, 171, 11, 2, 171, 53, 36, 66, 72, 122, 29, 58, 67, 33, 27, 196, 240, 235, 95, 9, 36, 3, 252, 82, 253, 252, 7, 33, 130, 101, 77, };
	bool seedOk = (seed.size() == sizeof(REF_SEED)) && equal(
		seed.data(),
		REF_SEED,
		sizeof(REF_SEED));
	if (!seedOk) {
		printf("Error: seed test failed\n");
		return false;
	}

#if VERBOSE_TESTS
	printBytes("seed : ", seed);
#endif

	// - setup argon params
	Argon2_Context ctx;
	uint8_t rawHash[ARGON2_HASH_LEN];
	setupAquaArgonCtx(ctx, seed, rawHash);

#if VERBOSE_TESTS	
	printf("\n- Argon params -\n");

	printBytes("password   : ", seed);
	{
		assert(ctx.ad == NULL && ctx.secret == NULL && ctx.salt == NULL);
		printf("salt       : %s\n", "");
		printf("secret     : %s\n", "");
		printf("data       : %s\n", "");
	}
	printf("time       : %d\n", ctx.t_cost);
	printf("mem        : %d\n", ctx.m_cost);
	printf("threads    : %d\n", ctx.threads);
	printf("HashLength : %d\n", ctx.outlen);
#endif

	// - test argon initial hash
#if VERBOSE_TESTS
	printf("\n- Initial Hash test -\n");
#endif

	uint8_t blockhash[ARGON2_PREHASH_SEED_LENGTH];
	initial_hash(blockhash, &ctx, Argon2_i);

	const uint8_t REF_H0[ARGON2_PREHASH_DIGEST_LENGTH] = { 97, 189, 62, 244, 247, 196, 145, 234, 110, 132, 189, 1, 247, 106, 121, 227, 238, 199, 204, 50, 130, 73, 27, 135, 255, 184, 130, 242, 164, 83, 36, 225, 249, 89, 83, 103, 37, 237, 84, 109, 52, 249, 57, 94, 119, 84, 76, 193, 244, 38, 86, 215, 189, 218, 233, 179, 96, 88, 241, 32, 81, 35, 117, 84, };
	bool hashOk = (ARGON2_PREHASH_DIGEST_LENGTH == sizeof(REF_H0)) && equal(
		blockhash,
		REF_H0,
		sizeof(REF_H0));
	if (!hashOk) {
		printf("Error: Initial hash test failed");
		return false;
	}

#if VERBOSE_TESTS
	printBytes("H0 :", blockhash, ARGON2_PREHASH_DIGEST_LENGTH);
#endif

	// - test argon2i
	int res;
#if VERBOSE_TESTS
	printf("\n- Argon2i test -\n");
#endif
	res = argon2_ctx(&ctx, Argon2_i);
	if (res != ARGON2_OK) {
		printf("Error: argon2_ctx failed (Argon2_i)");
		return false;
	}

	uint8_t REF_ARGON2I[] = { 204, 4, 18, 128, 226, 103, 56, 36, 254, 225, 79, 81, 158, 68, 162, 153, 176, 241, 21, 25, 31, 132, 86, 214, 84, 148, 77, 67, 189, 101, 153, 85, };
	bool argon2iOk = (ctx.outlen == sizeof(REF_ARGON2I)) && equal(
	ctx.out,
		REF_ARGON2I,
	sizeof(REF_ARGON2I));
	if (!argon2iOk) {
		printf("Error: argon2i test failed");
		return false;
	}
#if VERBOSE_TESTS
	printBytes("result: ", ctx.out, ctx.outlen);
#endif

	// - test argon2id
#if VERBOSE_TESTS
	printf("\n- Argon2id test -\n");
#endif
	res = argon2_ctx(&ctx, Argon2_id);
	if (res != ARGON2_OK) {
		printf("Error: argon2_ctx failed (Argon2_id)");
		return false;
	}

	uint8_t REF_ARGON2ID[] = { 211, 140, 184, 182, 141, 66, 195, 87, 238, 89, 96, 114, 228, 98, 27, 93, 236, 37, 243, 96, 225, 180, 23, 60, 177, 52, 161, 117, 42, 54, 244, 234, };
	bool argon2idOk = (ctx.outlen == sizeof(REF_ARGON2ID)) && equal(
		ctx.out,
		REF_ARGON2ID,
		sizeof(REF_ARGON2ID));
	if (!argon2idOk) {
		printf("Error: argon2id test failed");
		return false;
	}

	// test conversion of the result to a mpz
	mpz_t mpz_res;
	mpz_fromBytes(ctx.out, ctx.outlen, mpz_res);
	std::string resStr = mpzToString(mpz_res);
	const auto REF_RESULT = "95686644483052782493399538392398490716806085897040757836786893807315762738410";
	if (resStr != REF_RESULT) {
		printf("Error: mpz_fromBytes test failed");
		return false;
	}

#if VERBOSE_TESTS
	printBytes("result: ", ctx.out, ctx.outlen);
	printf("\n---- testAquaHashing() OK ----\n\n");
#endif
	
	// - mpz_import bench
	const bool BENCH_MPZ_IMPORT = false;
	if (BENCH_MPZ_IMPORT)
	{
		Timer tTotal;
		float durationS;
		const int N_ITER = 10000 * 1000;

		// 1: 2131, 2098
		// 2/4/8: ~1950
		tTotal.start();
		{
			mpz_t mpz_res;
			for (int i = 0; i < N_ITER; i++) {
				mpz_fromBytes(ctx.out, ctx.outlen, mpz_res);
			}
		}
		tTotal.end(durationS);
		printf("mpz_fromBytes took %.2fms\n", durationS*1000.f);

		// 1: 714, 702
		// 2/4/8: ~550
		tTotal.start();
		{
			mpz_t mpz_res;
			mpz_init(mpz_res);
			for (int i = 0; i < N_ITER; i++) {
				mpz_fromBytesNoInit(ctx.out, ctx.outlen, mpz_res);
			}		
		}
		tTotal.end(durationS);
		printf("mpz_fromBytesNoInit took %.2fms\n", durationS*1000.f); 
	}


	// - Blake2b bench
	const bool BENCH_BLAKE2B = false;
	//https://www.cryptologie.net/article/406/simd-instructions-in-go/

	if (BENCH_BLAKE2B)
	{		
/*		
		Timer tTotal;
		tTotal.start();
		Bytes seed;
		generateAquaSeed(NONCE, WORK_HASH_HEX, seed);

		Argon2_Context ctx;
		uint8_t rawHash[ARGON2_HASH_LEN];
		setupAquaArgonCtx(ctx, seed, rawHash);

		uint8_t blockhash[ARGON2_PREHASH_SEED_LENGTH];
		for (int i = 0; i < 100 * 1000; i++) {
			initial_hash(blockhash, &ctx, Argon2_i);
		}
		float durationMs = 0;
		tTotal.end(durationMs);
		printf("=====>%.2fsms", durationMs*1000.f);
*/
	}

	return true;
}
