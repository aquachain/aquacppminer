#include "hex_encode_utils.h"

#include <stdlib.h>
#include <assert.h>
#include <cstring>

std::string mpzToString(mpz_t num) {
	char buf[256]; // must be at least 64 (big numbers ...)
	auto ret = gmp_snprintf(buf, sizeof(buf), "%Zd", num);
	assert(ret < sizeof(buf));
	return buf;
}

std::pair<bool, Bytes> hexToBytes(std::string s) {
	Bytes res;
	if (strncmp(s.c_str(), "0x", 2) == 0)
		s = s.substr(2);

	if ((s.size() & 1) != 0) {
		return{ false, res };
	}
	for (size_t i = 0; i < s.size() / 2; i++) {
		unsigned int v;
		char n[3] = { s[i * 2], s[i * 2 + 1], 0 };
		int nRead = sscanf(n, "%x", &v);
		if (nRead != 1)
			return{ false, res };
		assert(v >= 0 && v <= 0xff);
		res.push_back((byte)v);
	}
	return{ true, res };
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
