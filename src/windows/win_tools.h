#pragma once

void setConsoleSize(int width, int height, int bufferHeight);

typedef void(*ctrlCFnPtr_t)(void);
bool setCtrlCHandler(ctrlCFnPtr_t pFn);