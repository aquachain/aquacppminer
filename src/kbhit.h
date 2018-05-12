#pragma once

#ifndef _MSC_VER
int kbhit(void);
#else
#include <conio.h>
#define kbhit _kbhit
#endif

