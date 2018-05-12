#include "string_utils.h"

#include <iostream>
#include <sstream>
#include <string>

#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>

// https://stackoverflow.com/questions/9435385/split-a-string-using-c11
std::vector<std::string> split(const std::string& s, char delim)
{
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> elems;
	while (std::getline(ss, item, delim)) {
		elems.push_back(std::move(item));
	}
	return elems;
}

// https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring

bool charIsValid(int c)
{
	return c >= -1 && c <= 255;
}

// trim from start
std::string ltrim(const std::string& s)
{
	std::string r = s;
	r.erase(
		r.begin(),
		std::find_if(r.begin(), r.end(), [](int c) -> int { return charIsValid(c) && !std::isspace(c); }));
	return r;
}

// trim from end
std::string rtrim(const std::string& s)
{
	std::string r = s;
	r.erase(
		std::find_if(
			r.rbegin(),
			r.rend(),
			[](int c) -> int { return charIsValid(c) && !std::isspace(c); })
			.base(),
		r.end());
	return r;
}

// trim from both ends
std::string trim(const std::string& s)
{
	return ltrim(rtrim(s));
}
