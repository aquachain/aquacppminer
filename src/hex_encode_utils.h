#pragma once

#include <string>
#include <gmp.h>

void decodeHex(const char* encoded, mpz_t mpz_res);
std::string decodeHex(const std::string &encoded);

void encodeHex(mpz_t mpz_num, std::string& res);

std::string mpzToString(mpz_t num);