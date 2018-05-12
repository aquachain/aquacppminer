#include <stdio.h>
#include <string>

std::string getPwd(char** argv) {
	std::string exePath(argv[0]);
	std::string exeDir;
#ifdef _MSC_VER
	auto indexLast = exePath.rfind("\\");
#else
	auto indexLast = exePath.rfind("/");
#endif
	return exePath.substr(0, indexLast + 1);
}