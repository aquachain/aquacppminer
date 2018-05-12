#pragma once

#include <string>
#include <vector>

std::vector<std::string> split(const std::string& s, char delim);
// trim from start
std::string ltrim(const std::string& s);
// trim from end
std::string rtrim(const std::string& s);
// trim from both ends
std::string trim(const std::string& s);

std::wstring stringToWstring(const std::string& t_str);
