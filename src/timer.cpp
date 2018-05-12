#include "timer.h"
#include <assert.h>

using std::chrono::high_resolution_clock;

void Timer::start()
{
	startT = high_resolution_clock::now();
}

void Timer::end(float &tgt)
{
	endT = high_resolution_clock::now();
	std::chrono::duration<float> duration = endT - startT;
	tgt = duration.count();
	assert(tgt >= 0.f);
}
