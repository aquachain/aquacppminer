#pragma once

struct HashReference {
	HashReference(const char* _argon2i, const char* _finalHash, const char* _duration, const char* _result);
	std::string getSaltBase64() const;

	std::string argon2i;
	std::string finalHash;
	std::string duration;
	std::string result;
};

bool testHasher(std::string &testsLog);
bool testSubmit(std::string &testsLog);