#pragma once

#include <stdarg.h>

void logLine(const char* prefix, const char* fmt, ...);
void logLine(std::string prefix, const char* fmt, ...);
