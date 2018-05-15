#include "hex_encode_utils.h"

#include <stdlib.h>

std::string mpzToString(mpz_t num) {
	char buf[64];
	gmp_snprintf(buf, sizeof(buf), "%Zd", num);
	return buf;
}

void decodeHex(const char* encoded, mpz_t mpz_res) {
	auto pStart = encoded;
	if (strncmp(encoded, "0x", 2) == 0)
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
