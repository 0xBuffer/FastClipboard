#pragma once
#include <windows.h>
#include <string>
#include "database.h"

namespace clipboard {

void Init(HWND hWnd);
void OnClipboardUpdate(HWND hWnd);
void PasteItem(const ClipboardItem& item, HWND targetWnd, HWND fastClipboardWnd);
std::wstring Utf8ToUtf16(const std::string& utf8);
std::string Utf16ToUtf8(const std::wstring& utf16);

}
