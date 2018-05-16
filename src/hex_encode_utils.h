#pragma once

#include <string>
#include <vector>
#include <gmp.h>

void decodeHex(const char* encoded, mpz_t mpz_res);
std::string decodeHex(const std::string &encoded);

void encodeHex(mpz_t mpz_num, std::string& res);

std::string mpzToString(mpz_t num);


typedef unsigned char byte;
typedef std::vector<byte> Bytes;

std::pair<bool, Bytes> hexToBytes(std::string s);