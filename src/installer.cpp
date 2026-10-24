#include "installer.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace installer {

std::string GetAppDataDirectory() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        fs::path p(path);
        p /= "FastClipboard";
        return p.string();
    }
    const char* appdata = getenv("APPDATA");
    if (appdata) {
        fs::path p(appdata);
        p /= "FastClipboard";
        return p.string();
    }
    return "FastClipboard_Data";
}

std::string GetImagesDirectory() {
    fs::path p(GetAppDataDirectory());
    p /= "images";
    return p.string();
}

std::string GetDatabasePath() {
    fs::path p(GetAppDataDirectory());
    p /= "clipboard.db";
    return p.string();
}

bool EnsureDirectoriesExist() {
    try {
        fs::path appDir = GetAppDataDirectory();
        if (!fs::exists(appDir)) {
            fs::create_directories(appDir);
        }
        fs::path imgDir = GetImagesDirectory();
        if (!fs::exists(imgDir)) {
            fs::create_directories(imgDir);
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string GetCurrentExecutablePath() {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    return std::string(exePath);
}

static std::string GetConfigFile() {
    fs::path p(GetAppDataDirectory());
    p /= "setup.flag";
    return p.string();
}

bool IsFirstRunCompleted() {
    fs::path flag = GetConfigFile();
    return fs::exists(flag);
}

void SetFirstRunCompleted(bool completed) {
    fs::path flag = GetConfigFile();
    EnsureDirectoriesExist();
    if (completed) {
        std::ofstream out(flag.string());
        if (out.is_open()) {
            out << "installed=1\n";
            out.close();
        }
    } else {
        std::error_code ec;
        fs::remove(flag, ec);
    }
}

static bool ExecuteHiddenCommand(const std::string& cmd) {
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<char> cmdVec(cmd.begin(), cmd.end());
    cmdVec.push_back('\0');

    if (CreateProcessA(NULL, cmdVec.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 8000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

static bool SetRegistryAutoRun(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            std::string exe = GetCurrentExecutablePath();
            std::string val = "\"" + exe + "\"";
            RegSetValueExA(hKey, "FastClipboard", 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)(val.size() + 1));
        } else {
            RegDeleteValueA(hKey, "FastClipboard");
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool IsScheduledTaskInstalled() {
    std::string cmd = "schtasks.exe /query /tn \"FastClipboard\"";
    return ExecuteHiddenCommand(cmd);
}

bool InstallScheduledTask() {
    std::string exe = GetCurrentExecutablePath();
    SetRegistryAutoRun(true);

    std::string psCmd = "powershell.exe -WindowStyle Hidden -NoProfile -Command \""
        "$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit 0 -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries; "
        "$a = New-ScheduledTaskAction -Execute '" + exe + "'; "
        "$t = New-ScheduledTaskTrigger -AtLogOn; "
        "Register-ScheduledTask -TaskName 'FastClipboard' -Action $a -Trigger $t -Settings $s -Force\"";

    bool ok = ExecuteHiddenCommand(psCmd);
    if (!ok) {
        std::string fallbackCmd = "schtasks.exe /create /tn \"FastClipboard\" /tr \"\\\"" + exe + "\\\"\" /sc onlogon /f";
        ok = ExecuteHiddenCommand(fallbackCmd);
    }
    return ok;
}

bool RemoveScheduledTask() {
    SetRegistryAutoRun(false);
    std::string cmd = "schtasks.exe /delete /tn \"FastClipboard\" /f";
    return ExecuteHiddenCommand(cmd);
}

}
