#pragma once

#include <string>

bool parseArgs(const char* prefix, int argc, char** argv);
void printUsage();
std::pair<bool, uint32_t> parseRefreshRate(const std::string& refreshRateStr);

const std::string OPT_USAGE = "-h";
const std::string OPT_NTHREADS = "-t";
const std::string OPT_GETWORK_URL = "-F";
const std::string OPT_FULLNODE_URL = "-n";
const std::string OPT_REFRESH_RATE = "-r";
const std::string OPT_SOLO = "--solo";
const std::string OPT_ARGON = "--argon";
const std::string OPT_PROXY = "--proxy";
const std::string OPT_ARGON_SUBMIT = "--submit";

const std::string s_usageMsg =
"aquacppminer.exe -F url [-t nThreads] [-n nodeUrl] [--solo] [-r refreshRate] [-h]\n"
"  -F url         : url of pool or node to mine on, if not specified, will pool mine to dev's aquabase\n"
"  -t nThreads    : number of threads to use (if not specified will use maximum logical threads available)\n"
"  -n node_url    : optional node url, to get more stats (pool mining only)\n"
"  -r rate        : pool refresh rate, ex: 3s, 2.5m, default is 3s\n"
"  --solo         : solo mining, -F needs to be the node url\n"
"  --proxy        : proxy to use, ex: --proxy socks5://127.0.0.1:9150"
"  --argon x,y,z  : use specific argon params (ex: 4,512,1), skip shares submit if incompatible with HF7\n"
"  --submit       : when used with --argon, forces submitting shares to pool/node\n"
"  -h             : display this help message and exit\n"
;