#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct ClipboardItem {
    int id = 0;
    std::string type;
    std::string content;
    std::string preview;
    int folder_id = 0;
    int64_t created_at = 0;
    int pinned = 0;
    int is_masked = 0;
};

struct FolderItem {
    int id = 0;
    std::string name;
    int count = 0;
};

class Database {
public:
    static Database& Instance();

    bool Init(const std::string& dbPath);
    void Close();

    bool AddItem(const std::string& type, const std::string& content, const std::string& preview, int folder_id = 0, int is_masked = 0);
    bool AddCustomItem(const std::string& content, int folder_id = 0, int is_masked = 0);
    bool DeleteItem(int id);
    bool MoveItemToFolder(int itemId, int folderId);
    bool ToggleMaskItem(int itemId, int isMasked);
    bool ClearAll(int folderId = -1);

    std::vector<ClipboardItem> GetItems(int folderId, const std::string& searchQuery = "");
    std::vector<FolderItem> GetFolders();
    bool CreateFolder(const std::string& name);
    bool DeleteFolder(int folderId);

    bool GetLastText(std::string& outText);

    bool SetSetting(const std::string& key, const std::string& val);
    std::string GetSetting(const std::string& key, const std::string& defaultVal = "");

private:
    Database();
    ~Database();
    struct sqlite3* m_db = nullptr;
};
