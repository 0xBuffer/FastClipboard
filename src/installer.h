#pragma once
#include <string>

namespace installer {

std::string GetAppDataDirectory();
std::string GetImagesDirectory();
std::string GetDatabasePath();
bool EnsureDirectoriesExist();
bool IsFirstRunCompleted();
void SetFirstRunCompleted(bool completed);
bool IsScheduledTaskInstalled();
bool InstallScheduledTask();
bool RemoveScheduledTask();
std::string GetCurrentExecutablePath();

}
