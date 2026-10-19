#pragma once
#include <windows.h>

#define WM_FASTCLIPBOARD_TOGGLE (WM_USER + 101)
#define WM_FASTCLIPBOARD_TRAY   (WM_USER + 102)

namespace hotkey {

bool InstallHook(HWND hWnd);
void UninstallHook();
void DetectTargetAndPosition(HWND& outTargetWnd, POINT& outPos, int winWidth, int winHeight);

}
