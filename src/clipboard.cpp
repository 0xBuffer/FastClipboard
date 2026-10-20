#include "clipboard.h"
#include "installer.h"
#include "database.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cmath>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

namespace clipboard {

static bool g_IgnoreNextUpdate = false;
static uint64_t g_LastImageHash = 0;
static ULONGLONG g_LastImageTick = 0;
static int g_LastImageWidth = 0;
static int g_LastImageHeight = 0;
static size_t g_LastDibSize = 0;
static DWORD g_LastClipboardSeq = 0;

static uint64_t HashMemory(const void* data, size_t size) {
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

std::wstring Utf8ToUtf16(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

std::string Utf16ToUtf8(const std::wstring& utf16) {
    if (utf16.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.size(), &result[0], size, nullptr, nullptr);
    return result;
}

void Init(HWND hWnd) {
    AddClipboardFormatListener(hWnd);
}

static std::string CreatePreview(const std::string& text) {
    std::string preview;
    int lineCount = 0;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') {
            lineCount++;
            if (lineCount >= 3 || preview.size() >= 120) break;
            preview += ' ';
            continue;
        }
        preview += c;
        if (preview.size() >= 120) break;
    }
    size_t first = preview.find_first_not_of(" \t");
    if (first != std::string::npos) {
        preview = preview.substr(first);
    }
    return preview;
}

static bool SaveDibToPng(const void* dibData, size_t dataSize, const std::string& outPngPath) {
    if (!dibData || dataSize < sizeof(BITMAPINFOHEADER)) return false;

    const BITMAPINFOHEADER* bmih = (const BITMAPINFOHEADER*)dibData;
    int width = bmih->biWidth;
    int height = bmih->biHeight;
    bool isTopDown = (height < 0);
    if (height < 0) height = -height;
    int bpp = bmih->biBitCount;

    if (width <= 0 || height <= 0 || (bpp != 24 && bpp != 32)) {
        return false;
    }

    size_t headerSize = bmih->biSize;
    if (bmih->biCompression == BI_BITFIELDS) {
        headerSize += 12;
    }
    const unsigned char* pixels = ((const unsigned char*)dibData) + headerSize;

    int rowPitch = ((width * bpp + 31) / 32) * 4;
    std::vector<unsigned char> rgba(width * height * 4);

    for (int y = 0; y < height; ++y) {
        int srcY = isTopDown ? y : (height - 1 - y);
        const unsigned char* srcRow = pixels + srcY * rowPitch;
        unsigned char* dstRow = rgba.data() + y * width * 4;

        for (int x = 0; x < width; ++x) {
            if (bpp == 32) {
                dstRow[x * 4 + 0] = srcRow[x * 4 + 2];
                dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
                dstRow[x * 4 + 2] = srcRow[x * 4 + 0];
                dstRow[x * 4 + 3] = srcRow[x * 4 + 3] ? srcRow[x * 4 + 3] : 255;
            } else if (bpp == 24) {
                dstRow[x * 4 + 0] = srcRow[x * 3 + 2];
                dstRow[x * 4 + 1] = srcRow[x * 3 + 1];
                dstRow[x * 4 + 2] = srcRow[x * 3 + 0];
                dstRow[x * 4 + 3] = 255;
            }
        }
    }

    int res = stbi_write_png(outPngPath.c_str(), width, height, 4, rgba.data(), width * 4);
    return (res != 0);
}

void OnClipboardUpdate(HWND hWnd) {
    if (g_IgnoreNextUpdate) {
        g_IgnoreNextUpdate = false;
        return;
    }

    DWORD seq = GetClipboardSequenceNumber();
    if (seq != 0 && seq == g_LastClipboardSeq) {
        return;
    }

    if (!OpenClipboard(hWnd)) {
        return;
    }

    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const wchar_t* pWText = (const wchar_t*)GlobalLock(hData);
            if (pWText) {
                std::string utf8 = Utf16ToUtf8(pWText);
                GlobalUnlock(hData);

                std::string trimmed = utf8;
                trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
                trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

                if (!trimmed.empty()) {
                    std::string lastText;
                    bool hasLast = Database::Instance().GetLastText(lastText);
                    if (!hasLast || lastText != utf8) {
                        std::string preview = CreatePreview(trimmed);
                        Database::Instance().AddItem("text", utf8, preview, 0, 0);
                        g_LastClipboardSeq = seq;
                    }
                }
            }
        }
    } else if (IsClipboardFormatAvailable(CF_DIB)) {
        HANDLE hData = GetClipboardData(CF_DIB);
        if (hData) {
            void* pDib = GlobalLock(hData);
            size_t dibSize = GlobalSize(hData);
            if (pDib && dibSize >= sizeof(BITMAPINFOHEADER)) {
                const BITMAPINFOHEADER* bmih = (const BITMAPINFOHEADER*)pDib;
                int width = bmih->biWidth;
                int height = std::abs(bmih->biHeight);

                uint64_t imgHash = HashMemory(pDib, dibSize);
                ULONGLONG nowTick = GetTickCount64();

                if (imgHash == g_LastImageHash || 
                    (width == g_LastImageWidth && height == g_LastImageHeight && dibSize == g_LastDibSize && (nowTick - g_LastImageTick < 2500))) {
                    GlobalUnlock(hData);
                    CloseClipboard();
                    return;
                }

                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                std::string filename = "clip_" + std::to_string(nowMs) + ".png";
                fs::path outPath = fs::path(installer::GetImagesDirectory()) / filename;

                if (SaveDibToPng(pDib, dibSize, outPath.string())) {
                    Database::Instance().AddItem("image", outPath.string(), "[Image]", 0, 0);
                    g_LastImageHash = imgHash;
                    g_LastImageTick = nowTick;
                    g_LastImageWidth = width;
                    g_LastImageHeight = height;
                    g_LastDibSize = dibSize;
                    g_LastClipboardSeq = seq;
                }
                GlobalUnlock(hData);
            } else if (pDib) {
                GlobalUnlock(hData);
            }
        }
    }

    CloseClipboard();
}

void PasteItem(const ClipboardItem& item, HWND targetWnd, HWND fastClipboardWnd) {
    g_IgnoreNextUpdate = true;

    if (OpenClipboard(fastClipboardWnd)) {
        EmptyClipboard();

        if (item.type == "text") {
            std::wstring wtext = Utf8ToUtf16(item.content);
            size_t bytes = (wtext.size() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (hMem) {
                void* p = GlobalLock(hMem);
                if (p) {
                    memcpy(p, wtext.c_str(), bytes);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
            }
        } else if (item.type == "image") {
            int width = 0, height = 0, channels = 0;
            unsigned char* img = stbi_load(item.content.c_str(), &width, &height, &channels, 4);
            if (img && width > 0 && height > 0) {
                int rowPitch = ((width * 32 + 31) / 32) * 4;
                size_t imageSize = rowPitch * height;
                size_t totalSize = sizeof(BITMAPINFOHEADER) + imageSize;

                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
                if (hMem) {
                    unsigned char* pData = (unsigned char*)GlobalLock(hMem);
                    if (pData) {
                        BITMAPINFOHEADER* bmih = (BITMAPINFOHEADER*)pData;
                        memset(bmih, 0, sizeof(BITMAPINFOHEADER));
                        bmih->biSize = sizeof(BITMAPINFOHEADER);
                        bmih->biWidth = width;
                        bmih->biHeight = height;
                        bmih->biPlanes = 1;
                        bmih->biBitCount = 32;
                        bmih->biCompression = BI_RGB;
                        bmih->biSizeImage = (DWORD)imageSize;

                        unsigned char* pPixels = pData + sizeof(BITMAPINFOHEADER);
                        for (int y = 0; y < height; ++y) {
                            int srcY = height - 1 - y;
                            const unsigned char* srcRow = img + srcY * width * 4;
                            unsigned char* dstRow = pPixels + y * rowPitch;
                            for (int x = 0; x < width; ++x) {
                                dstRow[x * 4 + 0] = srcRow[x * 4 + 2];
                                dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
                                dstRow[x * 4 + 2] = srcRow[x * 4 + 0];
                                dstRow[x * 4 + 3] = srcRow[x * 4 + 3];
                            }
                        }
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_DIB, hMem);
                    }
                }
                stbi_image_free(img);
            }
        }

        CloseClipboard();
    }

    ShowWindow(fastClipboardWnd, SW_HIDE);

    if (targetWnd && IsWindow(targetWnd)) {
        SetForegroundWindow(targetWnd);
        SetActiveWindow(targetWnd);
    }

    Sleep(50);

    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}

}
