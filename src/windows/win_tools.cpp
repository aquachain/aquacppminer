#include "win_tools.h"
#include <windows.h>

void setConsoleSize(int width, int height, int bufferHeight)
{
	_COORD coord;
	coord.X = width;
	coord.Y = bufferHeight; // height;

	_SMALL_RECT Rect;
	Rect.Top = 0;
	Rect.Left = 0;
	Rect.Bottom = height - 1;
	Rect.Right = width - 1;

	HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleScreenBufferSize(Handle, coord);
	SetConsoleWindowInfo(Handle, TRUE, &Rect);
}

ctrlCFnPtr_t s_ctrlcFn = nullptr;

BOOL WINAPI consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT) {
		if (s_ctrlcFn) {
			s_ctrlcFn();
		}
	}
	return TRUE;
}

bool setCtrlCHandler(ctrlCFnPtr_t pFn) {	
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
		return false;
	}
	s_ctrlcFn = pFn;
	return true;
}