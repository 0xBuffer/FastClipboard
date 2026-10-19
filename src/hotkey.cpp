#include "hotkey.h"

namespace hotkey {

static HHOOK g_hHook = NULL;
static HWND g_hAppWnd = NULL;
static HWND g_LastForegroundWnd = NULL;
static POINT g_LastCursorPos = { 0, 0 };

static bool g_WinVTriggered = false;
static ULONGLONG g_LAltDownTick = 0;
static bool g_LAltOtherKeyPressed = false;

static void TriggerOpen() {
    g_LastForegroundWnd = GetForegroundWindow();
    GetCursorPos(&g_LastCursorPos);
    if (g_hAppWnd && IsWindow(g_hAppWnd)) {
        PostMessage(g_hAppWnd, WM_FASTCLIPBOARD_TOGGLE, 0, 0);
    }
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (pKey->vkCode != VK_LMENU && pKey->vkCode != VK_RMENU && pKey->vkCode != VK_MENU) {
                g_LAltOtherKeyPressed = true;
            }

            if (pKey->vkCode == 'V' || pKey->vkCode == 'v') {
                bool winDown = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || 
                               ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
                bool laltDown = (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
                bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

                if (winDown && !ctrlDown && !laltDown) {
                    g_WinVTriggered = true;
                    keybd_event(0xE8, 0, 0, 0);
                    keybd_event(0xE8, 0, KEYEVENTF_KEYUP, 0);
                    TriggerOpen();
                    return 1;
                }

                if (laltDown && !ctrlDown && !winDown) {
                    TriggerOpen();
                    return 1;
                }
            }

            if (pKey->vkCode == VK_LMENU) {
                if (g_LAltDownTick == 0) {
                    g_LAltDownTick = GetTickCount64();
                    g_LAltOtherKeyPressed = false;
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (pKey->vkCode == VK_LWIN || pKey->vkCode == VK_RWIN) {
                if (g_WinVTriggered) {
                    g_WinVTriggered = false;
                    keybd_event(0xE8, 0, 0, 0);
                    keybd_event(0xE8, 0, KEYEVENTF_KEYUP, 0);
                    return 1;
                }
            }

            if (pKey->vkCode == VK_LMENU) {
                if (!g_LAltOtherKeyPressed && g_LAltDownTick > 0) {
                    ULONGLONG duration = GetTickCount64() - g_LAltDownTick;
                    g_LAltDownTick = 0;
                    if (duration <= 450) {
                        TriggerOpen();
                        return 1;
                    }
                }
                g_LAltDownTick = 0;
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

bool InstallHook(HWND hWnd) {
    g_hAppWnd = hWnd;
    if (!g_hHook) {
        g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    }
    return (g_hHook != NULL);
}

void UninstallHook() {
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    }
    g_hAppWnd = NULL;
}

void DetectTargetAndPosition(HWND& outTargetWnd, POINT& outPos, int winWidth, int winHeight) {
    if (g_LastForegroundWnd && IsWindow(g_LastForegroundWnd) && g_LastForegroundWnd != g_hAppWnd) {
        outTargetWnd = g_LastForegroundWnd;
    } else {
        HWND fg = GetForegroundWindow();
        if (fg && fg != g_hAppWnd) {
            outTargetWnd = fg;
        }
    }

    bool found = false;
    POINT pt = { 0, 0 };

    if (outTargetWnd && IsWindow(outTargetWnd)) {
        DWORD targetThread = GetWindowThreadProcessId(outTargetWnd, nullptr);
        DWORD currentThread = GetCurrentThreadId();
        bool attached = false;
        if (targetThread != currentThread) {
            attached = (AttachThreadInput(currentThread, targetThread, TRUE) != 0);
        }

        GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
        if (GetGUIThreadInfo(targetThread, &gti)) {
            HWND caretWnd = gti.hwndCaret ? gti.hwndCaret : gti.hwndFocus;
            int caretH = gti.rcCaret.bottom - gti.rcCaret.top;
            if (caretWnd && caretH >= 8 && caretH <= 80) {
                POINT caretPt = { gti.rcCaret.left, gti.rcCaret.bottom + 8 };
                if (ClientToScreen(caretWnd, &caretPt)) {
                    if (caretPt.x > -500 && caretPt.y > -500) {
                        pt = caretPt;
                        found = true;
                    }
                }
            }

            if (!found && gti.hwndFocus) {
                RECT rc = {};
                if (GetWindowRect(gti.hwndFocus, &rc)) {
                    int fw = rc.right - rc.left;
                    int fh = rc.bottom - rc.top;
                    if (fw >= 30 && fw <= 750 && fh >= 16 && fh <= 90) {
                        pt.x = rc.left;
                        pt.y = rc.bottom + 6;
                        found = true;
                    }
                }
            }
        }

        if (attached) {
            AttachThreadInput(currentThread, targetThread, FALSE);
        }
    }

    if (!found) {
        if (g_LastCursorPos.x != 0 || g_LastCursorPos.y != 0) {
            pt = g_LastCursorPos;
        } else {
            GetCursorPos(&pt);
        }
        pt.x += 12;
        pt.y += 18;
    }

    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMon, &mi);
    RECT work = mi.rcWork;

    if (pt.x + winWidth > work.right - 10) {
        pt.x = work.right - winWidth - 10;
    }
    if (pt.x < work.left + 10) {
        pt.x = work.left + 10;
    }

    if (pt.y + winHeight > work.bottom - 10) {
        int originY = found ? (pt.y - 8) : (pt.y - 18);
        pt.y = originY - winHeight - 10;
    }
    if (pt.y < work.top + 10) {
        pt.y = work.top + 10;
    }

    outPos = pt;
}

}
