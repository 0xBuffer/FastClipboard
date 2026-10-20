#include "database.h"
#include "sqlite3.h"
#include <chrono>

Database& Database::Instance() {
    static Database instance;
    return instance;
}

Database::Database() : m_db(nullptr) {}

Database::~Database() {
    Close();
}

bool Database::Init(const std::string& dbPath) {
    if (m_db) {
        return true;
    }

    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    sqlite3_exec(m_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);

    const char* schema = 
        "CREATE TABLE IF NOT EXISTS folders ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS items ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type TEXT NOT NULL,"
        "  content TEXT NOT NULL,"
        "  preview TEXT NOT NULL,"
        "  folder_id INTEGER DEFAULT 0,"
        "  created_at INTEGER NOT NULL,"
        "  pinned INTEGER DEFAULT 0,"
        "  is_masked INTEGER DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_items_folder ON items(folder_id);"
        "CREATE INDEX IF NOT EXISTS idx_items_created ON items(created_at DESC);";

    char* errMsg = nullptr;
    rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
        }
        return false;
    }

    sqlite3_exec(m_db, "ALTER TABLE items ADD COLUMN is_masked INTEGER DEFAULT 0;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, 
        "DELETE FROM items WHERE id IN ("
        "  SELECT i1.id FROM items i1 "
        "  JOIN items i2 ON i1.id > i2.id "
        "  AND i1.type = 'image' AND i2.type = 'image' "
        "  AND ABS(i1.created_at - i2.created_at) <= 2"
        ");", nullptr, nullptr, nullptr);

    return true;
}

void Database::Close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::AddItem(const std::string& type, const std::string& content, const std::string& preview, int folder_id, int is_masked) {
    if (!m_db) return false;

    const char* sql = "INSERT INTO items (type, content, preview, folder_id, created_at, pinned, is_masked) VALUES (?, ?, ?, ?, ?, 0, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, preview.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, folder_id);
    sqlite3_bind_int64(stmt, 5, now);
    sqlite3_bind_int(stmt, 6, is_masked);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::AddCustomItem(const std::string& content, int folder_id, int is_masked) {
    if (content.empty()) return false;
    std::string preview = content.substr(0, 120);
    return AddItem("text", content, preview, folder_id, is_masked);
}

bool Database::DeleteItem(int id) {
    if (!m_db) return false;

    const char* sql = "DELETE FROM items WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::MoveItemToFolder(int itemId, int folderId) {
    if (!m_db) return false;

    const char* sql = "UPDATE items SET folder_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, folderId);
    sqlite3_bind_int(stmt, 2, itemId);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::ToggleMaskItem(int itemId, int isMasked) {
    if (!m_db) return false;

    const char* sql = "UPDATE items SET is_masked = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, isMasked);
    sqlite3_bind_int(stmt, 2, itemId);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::ClearAll(int folderId) {
    if (!m_db) return false;

    sqlite3_stmt* stmt = nullptr;
    if (folderId < 0) {
        const char* sql = "DELETE FROM items;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    } else {
        const char* sql = "DELETE FROM items WHERE folder_id = ?;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, folderId);
    }

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<ClipboardItem> Database::GetItems(int folderId, const std::string& searchQuery) {
    std::vector<ClipboardItem> result;
    if (!m_db) return result;

    std::string sql = "SELECT id, type, content, preview, folder_id, created_at, pinned, is_masked FROM items WHERE 1=1";
    if (folderId > 0) {
        sql += " AND folder_id = ?";
    }
    if (!searchQuery.empty()) {
        sql += " AND (content LIKE ? OR preview LIKE ?)";
    }
    sql += " ORDER BY pinned DESC, created_at DESC LIMIT 200;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    int bindIdx = 1;
    if (folderId > 0) {
        sqlite3_bind_int(stmt, bindIdx++, folderId);
    }
    std::string likePattern;
    if (!searchQuery.empty()) {
        likePattern = "%" + searchQuery + "%";
        sqlite3_bind_text(stmt, bindIdx++, likePattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bindIdx++, likePattern.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ClipboardItem item;
        item.id = sqlite3_column_int(stmt, 0);
        item.type = (const char*)sqlite3_column_text(stmt, 1);
        item.content = (const char*)sqlite3_column_text(stmt, 2);
        item.preview = (const char*)sqlite3_column_text(stmt, 3);
        item.folder_id = sqlite3_column_int(stmt, 4);
        item.created_at = sqlite3_column_int64(stmt, 5);
        item.pinned = sqlite3_column_int(stmt, 6);
        item.is_masked = sqlite3_column_int(stmt, 7);
        result.push_back(item);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<FolderItem> Database::GetFolders() {
    std::vector<FolderItem> result;
    if (!m_db) return result;

    const char* sql = 
        "SELECT f.id, f.name, COUNT(i.id) AS cnt "
        "FROM folders f "
        "LEFT JOIN items i ON f.id = i.folder_id "
        "GROUP BY f.id, f.name "
        "ORDER BY f.name ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FolderItem f;
        f.id = sqlite3_column_int(stmt, 0);
        f.name = (const char*)sqlite3_column_text(stmt, 1);
        f.count = sqlite3_column_int(stmt, 2);
        result.push_back(f);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Database::CreateFolder(const std::string& name) {
    if (!m_db || name.empty()) return false;

    const char* sql = "INSERT INTO folders (name, created_at) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, now);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::DeleteFolder(int folderId) {
    if (!m_db || folderId <= 0) return false;

    const char* resetSql = "UPDATE items SET folder_id = 0 WHERE folder_id = ?;";
    sqlite3_stmt* stmt1 = nullptr;
    if (sqlite3_prepare_v2(m_db, resetSql, -1, &stmt1, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt1, 1, folderId);
        sqlite3_step(stmt1);
        sqlite3_finalize(stmt1);
    }

    const char* sql = "DELETE FROM folders WHERE id = ?;";
    sqlite3_stmt* stmt2 = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt2, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt2, 1, folderId);
    bool ok = (sqlite3_step(stmt2) == SQLITE_DONE);
    sqlite3_finalize(stmt2);
    return ok;
}

bool Database::GetLastText(std::string& outText) {
    if (!m_db) return false;

    const char* sql = "SELECT content FROM items WHERE type = 'text' ORDER BY created_at DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        outText = (const char*)sqlite3_column_text(stmt, 0);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool Database::SetSetting(const std::string& key, const std::string& val) {
    if (!m_db) return false;

    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, val.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::string Database::GetSetting(const std::string& key, const std::string& defaultVal) {
    if (!m_db) return defaultVal;

    const char* sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return defaultVal;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    std::string res = defaultVal;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        res = (const char*)sqlite3_column_text(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return res;
}
