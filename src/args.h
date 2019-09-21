#pragma once

#include <string>

bool parseArgs(const char* prefix, int argc, char** argv);
void printUsage();
std::pair<bool, uint32_t> parseRefreshRate(const std::string& refreshRateStr);

const std::string OPT_USAGE = "-h";
const std::string OPT_USAGE2 = "-help";
const std::string OPT_NTHREADS = "-t";
const std::string OPT_GETWORK_URL = "-F";
const std::string OPT_FULLNODE_URL = "-n";
const std::string OPT_REFRESH_RATE = "-r";
const std::string OPT_SOLO = "--solo";
const std::string OPT_PROXY = "--proxy";

const std::string s_usageMsg =
"aquacppminer.exe -F url [-t nThreads] [-n nodeUrl] [--solo] [-r refreshRate] [-h]\n"
"  -F url         : Mining URL. If not specified, will pool mine to local AQUA RPC server (port 8543)\n"
"  -t nThreads    : number of threads to use (if not specified will use maximum logical threads available)\n"
"  -n node_url    : optional node url, to get more stats (pool mining only)\n"
"  -r rate        : pool refresh rate in milliseconds, or 3s, 2.5m, default is 3s\n"
"  --solo         : solo mining, -F needs to be the node url or empty\n"
"  --proxy        : proxy to use, ex: --proxy socks5://127.0.0.1:9150\n"
"  -h             : display this help message and exit\n"
;

