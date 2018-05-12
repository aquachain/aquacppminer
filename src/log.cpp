#include <stdio.h>
#include <string>
#include <iostream>
#include <stdio.h>
#include <time.h>

#include "log.h"

void logLine(const char* prefix, const char* fmt, va_list args)
{
	char tmp[2048];
	vsnprintf(tmp, 2048, fmt, args);

	time_t  now = time(0);
	struct tm  tstruct;
	char bufTime[80];
	tstruct = *localtime(&now);
	strftime(bufTime, sizeof(bufTime), "%y-%m-%d %X", &tstruct);

	printf("[%s %s] %s\n", prefix, bufTime, tmp);

#ifdef _MSC_VER
	// workaround to show output when running in github windows console (minTTy)
	// see: https://github.com/mintty/mintty/issues/218
	fflush(stdout);
#endif
}

void logLine(const char* prefix, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logLine(prefix, fmt, args);
	va_end(args);
}
