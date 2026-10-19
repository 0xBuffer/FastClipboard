#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_set>
#include "database.h"
#include "imgui.h"

extern ImFont* g_FontDefault;
extern ImFont* g_FontSmall;

class App {
public:
    static App& Instance();

    void Init();
    void Render();

    void OnShow(HWND targetWnd);
    void OnHide();

    bool IsFirstRun() const { return m_isFirstRun; }
    void RequestClose() { m_requestClose = true; }
    bool ShouldClose() {
        bool val = m_requestClose;
        m_requestClose = false;
        return val;
    }

    void ReloadData();

private:
    App();
    ~App();

    void RenderFirstRunSetup();
    void RenderMainView();
    void RenderTopBar();
    void RenderItemsList();
    void RenderSideDrawer();
    void RenderCreateFolderModal();
    void RenderCreateItemModal();
    void RenderItemContextMenu(const ClipboardItem& item);

    bool m_isFirstRun = false;
    bool m_autostartCheckbox = true;
    bool m_requestClose = false;

    HWND m_targetWnd = NULL;
    int m_activeFolderId = 0;
    std::string m_activeFolderName;
    char m_searchBuffer[256] = { 0 };

    bool m_sideMenuOpen = false;
    float m_sideMenuAnim = 0.0f;
    float m_targetScrollY = 0.0f;

    bool m_showCreateFolderModal = false;
    char m_newFolderName[128] = { 0 };

    bool m_showCreateItemModal = false;
    char m_newItemContent[2048] = { 0 };
    int m_newItemFolderId = 0;
    bool m_newItemMasked = false;

    std::unordered_set<int> m_expandedItemIds;
    int m_contextMenuItemId = -1;

    std::vector<ClipboardItem> m_items;
    std::vector<FolderItem> m_folders;
};
