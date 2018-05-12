#include "formatDuration.h"

std::string formatDuration(float nSeconds)
{
	char res[128];
	if (nSeconds<60) {
		sprintf(res, "%4.1fs", nSeconds);
	}
	else if (nSeconds<3600.f) {
		sprintf(res, "%4.1fm", nSeconds / 60.f);
	}
	else if (nSeconds<(24 * 3600.f)) {
		sprintf(res, "%4.1fh", nSeconds / 3600.f);
	}
	else {
		sprintf(res, "%4.1fd", nSeconds / (24.f*3600.f));
	}
	return res;
}
