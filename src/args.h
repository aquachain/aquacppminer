#pragma once

#include <string>

bool parseArgs(const char* prefix, int argc, char** argv);
void printUsage();
std::pair<bool, uint32_t> parseRefreshRate(const std::string& refreshRateStr);

