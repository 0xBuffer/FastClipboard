#include "app.h"
#include "installer.h"
#include "database.h"
#include "clipboard.h"
#include "vk_texture.h"
#include "i18n.h"
#include "imgui.h"
#include <cstdio>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <cmath>

extern HWND g_hWnd;

App& App::Instance() {
    static App instance;
    return instance;
}

App::App() {
    m_activeFolderName = I18N().allItems;
}

App::~App() {}

void App::Init() {
    m_isFirstRun = !installer::IsFirstRunCompleted();
    m_activeFolderName = I18N().allItems;

    if (!m_isFirstRun) {
        installer::EnsureDirectoriesExist();
        Database::Instance().Init(installer::GetDatabasePath());
        ReloadData();
    }
}

void App::ReloadData() {
    m_items = Database::Instance().GetItems(m_activeFolderId, m_searchBuffer);
    m_folders = Database::Instance().GetFolders();
}

void App::OnShow(HWND targetWnd) {
    m_targetWnd = targetWnd;
    m_searchBuffer[0] = '\0';
    m_sideMenuOpen = false;
    m_showCreateFolderModal = false;
    m_showCreateItemModal = false;
    m_targetScrollY = 0.0f;
    ReloadData();
}

void App::OnHide() {
    m_sideMenuOpen = false;
    m_showCreateFolderModal = false;
    m_showCreateItemModal = false;
}

static std::string FormatTimestamp(int64_t timestamp) {
    if (timestamp <= 0) return "";
    time_t t = (time_t)timestamp;
    tm local;
    localtime_s(&local, &t);

    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M", &local);
    return std::string(buf);
}

static inline int GetUtf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static bool CheckHasMoreLines(const std::string& text, int lineLimit) {
    int lineCount = 1;
    int col = 0;
    const int maxCharsPerLine = 40;

    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = (unsigned char)text[i];
        int charLen = GetUtf8CharLen(c);
        if (i + charLen > text.size()) break;

        if (c == '\r') {
            i += charLen;
            continue;
        }
        if (c == '\n') {
            lineCount++;
            col = 0;
            if (lineCount > lineLimit) return true;
            i += charLen;
            continue;
        }

        col++;
        if (col >= maxCharsPerLine) {
            lineCount++;
            col = 0;
            if (lineCount > lineLimit) return true;
        }
        i += charLen;
    }
    return false;
}

static std::string MaskText(const std::string& text) {
    if (text.empty()) return "****";
    size_t len = text.size();
    if (len <= 2) return "****";
    if (len <= 4) {
        return text.substr(0, 1) + "***" + text.substr(len - 1, 1);
    }
    return text.substr(0, 2) + "****" + text.substr(len - 2, 2);
}

static std::string FormatCardText(const std::string& text, int maxLines) {
    std::string result;
    int lineCount = 1;
    int col = 0;
    const int maxCharsPerLine = 40;

    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = (unsigned char)text[i];
        int charLen = GetUtf8CharLen(c);
        if (i + charLen > text.size()) break;

        if (c == '\r') {
            i += charLen;
            continue;
        }
        if (c == '\n') {
            lineCount++;
            col = 0;
            if (lineCount > maxLines) {
                result += "...";
                break;
            }
            result += '\n';
            i += charLen;
            continue;
        }

        col++;
        if (col >= maxCharsPerLine) {
            lineCount++;
            col = 0;
            if (lineCount > maxLines) {
                result += "...";
                break;
            }
            result += '\n';
        }

        result.append(text, i, charLen);
        i += charLen;
    }
    return result;
}

void App::Render() {
    if (m_isFirstRun) {
        RenderFirstRunSetup();
    } else {
        RenderMainView();
    }
}

void App::RenderFirstRunSetup() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.125f, 0.125f, 0.125f, 0.98f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));

    ImGui::Begin("SetupWindow", nullptr, flags);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRect(ImVec2(0, 0), io.DisplaySize, IM_COL32(255, 255, 255, 25), 10.0f, 0, 1.2f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.70f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.25f);
    ImGui::Text("%s", I18N().setupWelcome);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextWrapped("%s", I18N().setupDesc);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox(I18N().setupAutostart, &m_autostartCheckbox);
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.70f, 1.0f));
    ImGui::TextWrapped("%s", I18N().setupHotkeyHint);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float btnW = 160.0f;
    float btnH = 36.0f;
    ImGui::SetCursorPosX((io.DisplaySize.x - btnW) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.47f, 0.83f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 0.92f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.40f, 0.72f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button(I18N().setupStartBtn, ImVec2(btnW, btnH))) {
        installer::EnsureDirectoriesExist();
        Database::Instance().Init(installer::GetDatabasePath());

        if (m_autostartCheckbox) {
            installer::InstallScheduledTask();
        } else {
            installer::RemoveScheduledTask();
        }

        installer::SetFirstRunCompleted(true);
        m_isFirstRun = false;
        ReloadData();
        RequestClose();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void App::RenderMainView() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.125f, 0.125f, 0.125f, 0.98f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

    ImGui::Begin("FastClipboardMain", nullptr, flags);

    ImDrawList* bgDrawList = ImGui::GetWindowDrawList();
    bgDrawList->AddRect(ImVec2(0, 0), io.DisplaySize, IM_COL32(255, 255, 255, 22), 10.0f, 0, 1.2f);

    if (ImGui::IsMouseDown(0) && !ImGui::IsAnyItemActive() && !m_sideMenuOpen && !m_showCreateFolderModal && !m_showCreateItemModal) {
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.y >= 0.0f && mousePos.y <= 44.0f) {
            ReleaseCapture();
            SendMessage(g_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
    }

    RenderTopBar();
    RenderItemsList();

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    float targetAnim = m_sideMenuOpen ? 1.0f : 0.0f;
    float dt = ImGui::GetIO().DeltaTime;
    m_sideMenuAnim += (targetAnim - m_sideMenuAnim) * std::min(dt * 18.0f, 1.0f);

    if (m_sideMenuAnim > 0.005f) {
        RenderSideDrawer();
    }

    if (m_showCreateFolderModal) {
        RenderCreateFolderModal();
    }

    if (m_showCreateItemModal) {
        RenderCreateItemModal();
    }
}

void App::RenderTopBar() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float barHeight = 32.0f;

    ImVec2 menuBtnPos = ImGui::GetCursorScreenPos();
    ImVec2 menuBtnSize(30, barHeight);
    bool menuClicked = ImGui::InvisibleButton("##HamburgerBtn", menuBtnSize);
    bool menuHovered = ImGui::IsItemHovered();

    ImVec2 menuCenter(menuBtnPos.x + menuBtnSize.x * 0.5f, menuBtnPos.y + menuBtnSize.y * 0.5f);
    if (menuHovered) {
        drawList->AddCircleFilled(menuCenter, 13.0f, IM_COL32(255, 255, 255, 20));
    }

    ImU32 menuCol = menuHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 190, 190, 230);
    float lineHalfW = 6.0f;
    float lThickness = 1.7f;
    drawList->AddLine(ImVec2(menuCenter.x - lineHalfW, menuCenter.y - 4.5f),
                      ImVec2(menuCenter.x + lineHalfW, menuCenter.y - 4.5f), menuCol, lThickness);
    drawList->AddLine(ImVec2(menuCenter.x - lineHalfW, menuCenter.y),
                      ImVec2(menuCenter.x + lineHalfW, menuCenter.y), menuCol, lThickness);
    drawList->AddLine(ImVec2(menuCenter.x - lineHalfW, menuCenter.y + 4.5f),
                      ImVec2(menuCenter.x + lineHalfW, menuCenter.y + 4.5f), menuCol, lThickness);

    if (menuClicked) {
        m_sideMenuOpen = !m_sideMenuOpen;
    }
    if (menuHovered) {
        ImGui::SetTooltip("%s", I18N().tooltipFolders);
    }

    ImGui::SameLine(0, 8.0f);

    float actionBtnsWidth = 66.0f;
    float searchW = ImGui::GetContentRegionAvail().x - actionBtnsWidth - 8.0f;
    ImGui::SetNextItemWidth(searchW);

    ImVec2 searchCursor = ImGui::GetCursorPos();
    searchCursor.y += 3.0f;
    ImGui::SetCursorPos(searchCursor);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.165f, 0.165f, 0.165f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.200f, 0.200f, 0.200f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.220f, 0.220f, 0.220f, 1.0f));

    char hintBuf[128];
    snprintf(hintBuf, sizeof(hintBuf), I18N().searchHint, m_activeFolderName.c_str());
    if (ImGui::InputTextWithHint("##Search", hintBuf, m_searchBuffer, sizeof(m_searchBuffer))) {
        ReloadData();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImGui::SameLine(0, 8.0f);

    ImVec2 plusBtnPos = ImGui::GetCursorScreenPos();
    ImVec2 plusBtnSize(28, barHeight);
    bool plusClicked = ImGui::InvisibleButton("##PlusBtn", plusBtnSize);
    bool plusHovered = ImGui::IsItemHovered();

    ImVec2 plusCenter(plusBtnPos.x + plusBtnSize.x * 0.5f, plusBtnPos.y + plusBtnSize.y * 0.5f);
    if (plusHovered) {
        drawList->AddCircleFilled(plusCenter, 13.0f, IM_COL32(0, 120, 212, 50));
    }
    ImU32 plusCol = plusHovered ? IM_COL32(96, 179, 255, 255) : IM_COL32(180, 180, 180, 220);
    drawList->AddLine(ImVec2(plusCenter.x - 5.0f, plusCenter.y),
                      ImVec2(plusCenter.x + 5.0f, plusCenter.y), plusCol, 1.8f);
    drawList->AddLine(ImVec2(plusCenter.x, plusCenter.y - 5.0f),
                      ImVec2(plusCenter.x, plusCenter.y + 5.0f), plusCol, 1.8f);

    if (plusClicked) {
        m_showCreateItemModal = true;
        m_newItemContent[0] = '\0';
        m_newItemFolderId = m_activeFolderId;
        m_newItemMasked = false;
    }
    if (plusHovered) {
        ImGui::SetTooltip("%s", I18N().tooltipNewItem);
    }

    ImGui::SameLine(0, 4.0f);

    ImVec2 closeBtnPos = ImGui::GetCursorScreenPos();
    ImVec2 closeBtnSize(28, barHeight);
    bool closeClicked = ImGui::InvisibleButton("##CloseBtn", closeBtnSize);
    bool closeHovered = ImGui::IsItemHovered();

    ImVec2 closeCenter(closeBtnPos.x + closeBtnSize.x * 0.5f, closeBtnPos.y + closeBtnSize.y * 0.5f);
    if (closeHovered) {
        drawList->AddCircleFilled(closeCenter, 13.0f, IM_COL32(239, 68, 68, 55));
    }

    ImU32 closeCol = closeHovered ? IM_COL32(248, 113, 113, 255) : IM_COL32(180, 180, 180, 220);
    float crossR = 4.5f;
    drawList->AddLine(ImVec2(closeCenter.x - crossR, closeCenter.y - crossR),
                      ImVec2(closeCenter.x + crossR, closeCenter.y + crossR), closeCol, 1.7f);
    drawList->AddLine(ImVec2(closeCenter.x + crossR, closeCenter.y - crossR),
                      ImVec2(closeCenter.x - crossR, closeCenter.y + crossR), closeCol, 1.7f);

    if (closeClicked) {
        RequestClose();
    }
    if (closeHovered) {
        ImGui::SetTooltip("%s", I18N().tooltipClose);
    }

    ImGui::Spacing();
}

void App::RenderItemsList() {
    if (m_activeFolderId != 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.70f, 1.0f, 1.0f));
        ImGui::Text("%s%s", I18N().folderPrefix, m_activeFolderName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton(I18N().showAll)) {
            m_activeFolderId = 0;
            m_activeFolderName = I18N().allItems;
            ReloadData();
        }
        ImGui::Spacing();
    }

    if (m_items.empty()) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.60f, 1.0f));
        float winW = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX((winW - ImGui::CalcTextSize(I18N().emptyTitle).x) * 0.5f);
        ImGui::Text("%s", I18N().emptyTitle);
        ImGui::SetCursorPosX((winW - ImGui::CalcTextSize(I18N().emptyDesc).x) * 0.5f);
        ImGui::Text("%s", I18N().emptyDesc);
        ImGui::PopStyleColor();
        return;
    }

    ImGui::BeginChild("CardsListChild", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
        m_targetScrollY -= wheel * 95.0f;
    }
    float maxScroll = ImGui::GetScrollMaxY();
    m_targetScrollY = std::clamp(m_targetScrollY, 0.0f, maxScroll);

    float curScroll = ImGui::GetScrollY();
    if (std::abs(m_targetScrollY - curScroll) > 0.4f) {
        float nextScroll = curScroll + (m_targetScrollY - curScroll) * std::min(ImGui::GetIO().DeltaTime * 16.0f, 1.0f);
        ImGui::SetScrollY(nextScroll);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float availW = ImGui::GetContentRegionAvail().x;

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        ImGui::PushID(item.id);

        bool isImage = (item.type == "image");
        bool isExpanded = (m_expandedItemIds.find(item.id) != m_expandedItemIds.end());
        bool hasMore = !isImage && CheckHasMoreLines(item.content, 3);

        float cardH = isImage ? 90.0f : (isExpanded ? 172.0f : 76.0f);
        ImVec2 curPos = ImGui::GetCursorScreenPos();

        float dotZoneW = 30.0f;
        float mainZoneW = availW - dotZoneW - 2.0f;

        bool cardClicked = ImGui::InvisibleButton("##CardPasteZone", ImVec2(mainZoneW, cardH));
        bool cardHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_None);

        ImGui::SameLine(0, 2.0f);

        float dotBtnH = 24.0f;
        bool dotClicked = ImGui::InvisibleButton("##CardDotZone", ImVec2(dotZoneW, dotBtnH));
        bool dotHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_None);

        ImVec2 fullCardMax(curPos.x + availW, curPos.y + cardH);
        bool anyHovered = cardHovered || dotHovered;

        ImU32 bgCol = anyHovered ? IM_COL32(50, 50, 50, 255) : IM_COL32(43, 43, 43, 255);
        ImU32 borderCol = anyHovered ? IM_COL32(255, 255, 255, 45) : IM_COL32(255, 255, 255, 18);

        drawList->AddRectFilled(curPos, fullCardMax, bgCol, 8.0f);
        drawList->AddRect(curPos, fullCardMax, borderCol, 8.0f, 0, 1.0f);

        std::string timeStr = FormatTimestamp(item.created_at);
        if (g_FontSmall) ImGui::PushFont(g_FontSmall);
        drawList->AddText(ImVec2(curPos.x + 10.0f, curPos.y + 6.0f), IM_COL32(155, 155, 155, 255), timeStr.c_str());

        if (item.is_masked) {
            float timeW = ImGui::CalcTextSize(timeStr.c_str()).x;
            drawList->AddText(ImVec2(curPos.x + 16.0f + timeW, curPos.y + 6.0f), IM_COL32(234, 179, 8, 255), "[*]");
        }
        if (g_FontSmall) ImGui::PopFont();

        float dotY = curPos.y + 12.0f;
        float dotCenterX = curPos.x + availW - 15.0f;
        if (dotHovered) {
            drawList->AddRectFilled(ImVec2(dotCenterX - 11.0f, dotY - 7.0f),
                                   ImVec2(dotCenterX + 11.0f, dotY + 7.0f),
                                   IM_COL32(255, 255, 255, 24), 4.0f);
        }
        ImU32 dotCol = dotHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(170, 170, 170, 220);
        drawList->AddCircleFilled(ImVec2(dotCenterX - 5.5f, dotY), 1.6f, dotCol);
        drawList->AddCircleFilled(ImVec2(dotCenterX,        dotY), 1.6f, dotCol);
        drawList->AddCircleFilled(ImVec2(dotCenterX + 5.5f, dotY), 1.6f, dotCol);

        if (isImage) {
            int imgW = 0, imgH = 0;
            ImTextureID texId = vk_texture::GetOrCreateTexture(item.content, imgW, imgH);
            if (texId) {
                float maxThumbH = 62.0f;
                float aspect = (imgH > 0) ? ((float)imgW / (float)imgH) : 1.0f;
                float thumbW = std::clamp(maxThumbH * aspect, 32.0f, 150.0f);
                float thumbH = maxThumbH;

                ImVec2 imgPos(curPos.x + 10.0f, curPos.y + 22.0f);
                drawList->AddImage(texId, imgPos, ImVec2(imgPos.x + thumbW, imgPos.y + thumbH),
                                   ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
                drawList->AddRect(imgPos, ImVec2(imgPos.x + thumbW, imgPos.y + thumbH),
                                  IM_COL32(255, 255, 255, 35), 4.0f);
            }
        } else {
            int maxLines = isExpanded ? 8 : 3;
            std::string contentToDisplay = item.is_masked ? MaskText(item.content) : item.content;
            std::string displayText = FormatCardText(contentToDisplay, maxLines);
            drawList->AddText(ImVec2(curPos.x + 10.0f, curPos.y + 22.0f),
                              IM_COL32(245, 245, 245, 255), displayText.c_str());

            if (hasMore && !item.is_masked) {
                float expandBtnW = 20.0f;
                float expandBtnH = 18.0f;
                ImVec2 expandPos(curPos.x + availW - expandBtnW - 8.0f, curPos.y + cardH - expandBtnH - 4.0f);

                ImVec2 mousePos = ImGui::GetMousePos();
                bool expandHovered = (mousePos.x >= expandPos.x && mousePos.x <= expandPos.x + expandBtnW &&
                                      mousePos.y >= expandPos.y && mousePos.y <= expandPos.y + expandBtnH);

                if (expandHovered) {
                    drawList->AddRectFilled(expandPos, ImVec2(expandPos.x + expandBtnW, expandPos.y + expandBtnH),
                                           IM_COL32(255, 255, 255, 25), 4.0f);
                }

                ImU32 chevCol = expandHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 200);
                float chevMidX = expandPos.x + expandBtnW * 0.5f;
                float chevMidY = expandPos.y + expandBtnH * 0.5f;

                if (isExpanded) {
                    drawList->AddLine(ImVec2(chevMidX - 4.0f, chevMidY + 2.0f), ImVec2(chevMidX, chevMidY - 2.0f), chevCol, 1.5f);
                    drawList->AddLine(ImVec2(chevMidX, chevMidY - 2.0f), ImVec2(chevMidX + 4.0f, chevMidY + 2.0f), chevCol, 1.5f);
                } else {
                    drawList->AddLine(ImVec2(chevMidX - 4.0f, chevMidY - 2.0f), ImVec2(chevMidX, chevMidY + 2.0f), chevCol, 1.5f);
                    drawList->AddLine(ImVec2(chevMidX, chevMidY + 2.0f), ImVec2(chevMidX + 4.0f, chevMidY - 2.0f), chevCol, 1.5f);
                }

                if (expandHovered && ImGui::IsMouseClicked(0)) {
                    if (isExpanded) m_expandedItemIds.erase(item.id);
                    else m_expandedItemIds.insert(item.id);
                }
            }
        }

        if (dotClicked) {
            m_contextMenuItemId = item.id;
            ImGui::OpenPopup("ItemActionPopup");
        }

        if (m_contextMenuItemId == item.id) {
            RenderItemContextMenu(item);
        }

        if (cardClicked && !m_sideMenuOpen && m_contextMenuItemId != item.id) {
            clipboard::PasteItem(item, m_targetWnd, g_hWnd);
        }

        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void App::RenderItemContextMenu(const ClipboardItem& item) {
    if (ImGui::BeginPopup("ItemActionPopup")) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.70f, 1.0f, 1.0f));
        ImGui::Text("%s", I18N().actionsTitle);
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (item.type == "text") {
            const char* maskLabel = item.is_masked ? I18N().actionUnmask : I18N().actionMask;
            if (ImGui::MenuItem(maskLabel)) {
                Database::Instance().ToggleMaskItem(item.id, item.is_masked ? 0 : 1);
                m_contextMenuItemId = -1;
                ReloadData();
                ImGui::EndPopup();
                return;
            }
            ImGui::Separator();
        }

        if (ImGui::MenuItem(I18N().actionDelete)) {
            Database::Instance().DeleteItem(item.id);
            m_contextMenuItemId = -1;
            ReloadData();
            ImGui::EndPopup();
            return;
        }

        if (ImGui::BeginMenu(I18N().actionMoveToFolder)) {
            if (ImGui::MenuItem(I18N().actionNoFolder, nullptr, item.folder_id == 0)) {
                Database::Instance().MoveItemToFolder(item.id, 0);
                m_contextMenuItemId = -1;
                ReloadData();
            }
            ImGui::Separator();

            for (const auto& f : m_folders) {
                bool isCur = (item.folder_id == f.id);
                if (ImGui::MenuItem(f.name.c_str(), nullptr, isCur)) {
                    Database::Instance().MoveItemToFolder(item.id, f.id);
                    m_contextMenuItemId = -1;
                    ReloadData();
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    } else {
        if (m_contextMenuItemId == item.id) {
            m_contextMenuItemId = -1;
        }
    }
}

void App::RenderSideDrawer() {
    ImGuiIO& io = ImGui::GetIO();
    float drawerWidth = 250.0f;
    float fullHeight = io.DisplaySize.y;

    float currentX = -drawerWidth * (1.0f - m_sideMenuAnim);
    float visibleW = drawerWidth + currentX;

    ImDrawList* fgDrawList = ImGui::GetForegroundDrawList();
    if (visibleW > 0.0f) {
        int alpha = (int)(130.0f * m_sideMenuAnim);
        fgDrawList->AddRectFilled(ImVec2(visibleW, 0), io.DisplaySize, IM_COL32(0, 0, 0, alpha));
    }

    if (ImGui::IsMouseClicked(0) && m_sideMenuOpen) {
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x > drawerWidth) {
            m_sideMenuOpen = false;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(currentX, 0));
    ImGui::SetNextWindowSize(ImVec2(drawerWidth, fullHeight));

    ImGuiWindowFlags drawerFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.140f, 0.140f, 0.140f, 0.99f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));

    ImGui::Begin("SideDrawerWindow", nullptr, drawerFlags);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddLine(ImVec2(drawerWidth - 1.0f, 0), ImVec2(drawerWidth - 1.0f, fullHeight), IM_COL32(255, 255, 255, 25), 1.5f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.70f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("%s", I18N().foldersTitle);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine(drawerWidth - 40.0f);
    if (ImGui::Button("<##CloseDrawer", ImVec2(28, 26))) {
        m_sideMenuOpen = false;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.47f, 0.83f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 0.92f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.40f, 0.72f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button(I18N().btnCreateFolder, ImVec2(-1, 34))) {
        m_showCreateFolderModal = true;
        m_newFolderName[0] = '\0';
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("DrawerFolderList", ImVec2(0, 0), false);

    bool isAllSelected = (m_activeFolderId == 0);
    if (isAllSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.47f, 0.83f, 0.9f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 0.8f));
    }

    if (ImGui::Button(I18N().allItems, ImVec2(-1, 34))) {
        m_activeFolderId = 0;
        m_activeFolderName = I18N().allItems;
        m_sideMenuOpen = false;
        ReloadData();
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    for (const auto& folder : m_folders) {
        ImGui::PushID(folder.id);
        bool isSelected = (m_activeFolderId == folder.id);

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.47f, 0.83f, 0.9f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 0.8f));
        }

        std::string label = folder.name + " (" + std::to_string(folder.count) + ")";
        float delBtnW = 28.0f;
        float btnW = ImGui::GetContentRegionAvail().x - delBtnW - 6.0f;

        if (ImGui::Button(label.c_str(), ImVec2(btnW, 34))) {
            m_activeFolderId = folder.id;
            m_activeFolderName = folder.name;
            m_sideMenuOpen = false;
            ReloadData();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.16f, 0.18f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.15f, 0.15f, 1.0f));

        if (ImGui::Button("X", ImVec2(delBtnW, 34))) {
            Database::Instance().DeleteFolder(folder.id);
            if (m_activeFolderId == folder.id) {
                m_activeFolderId = 0;
                m_activeFolderName = I18N().allItems;
            }
            ReloadData();
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void App::RenderCreateFolderModal() {
    ImGuiIO& io = ImGui::GetIO();
    float modalW = 280.0f;
    float modalH = 150.0f;

    ImVec2 modalPos((io.DisplaySize.x - modalW) * 0.5f, (io.DisplaySize.y - modalH) * 0.35f);
    ImGui::SetNextWindowPos(modalPos);
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.38f, 0.70f, 1.0f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));

    ImGui::Begin(I18N().modalCreateFolderTitle, &m_showCreateFolderModal, flags);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##FolderNameInput", I18N().folderNamePlaceholder, m_newFolderName, sizeof(m_newFolderName));

    ImGui::Spacing();
    ImGui::Spacing();

    float halfBtnW = (modalW - 40.0f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.47f, 0.83f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 0.92f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    if (ImGui::Button(I18N().btnCreate, ImVec2(halfBtnW, 32))) {
        if (m_newFolderName[0] != '\0') {
            Database::Instance().CreateFolder(m_newFolderName);
            ReloadData();
            m_showCreateFolderModal = false;
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    if (ImGui::Button(I18N().btnCancel, ImVec2(halfBtnW, 32))) {
        m_showCreateFolderModal = false;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void App::RenderCreateItemModal() {
    ImGuiIO& io = ImGui::GetIO();
    float modalW = 340.0f;
    float modalH = 260.0f;

    ImVec2 modalPos((io.DisplaySize.x - modalW) * 0.5f, (io.DisplaySize.y - modalH) * 0.35f);
    ImGui::SetNextWindowPos(modalPos);
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.38f, 0.70f, 1.0f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));

    ImGui::Begin(I18N().modalCreateItemTitle, &m_showCreateItemModal, flags);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##NewItemContent", m_newItemContent, sizeof(m_newItemContent),
                              ImVec2(-1, 80));

    ImGui::Spacing();

    ImGui::Text("%s", I18N().itemFolderLabel);
    ImGui::SetNextItemWidth(-1);

    std::string previewFolder = I18N().allItems;
    if (m_newItemFolderId > 0) {
        for (const auto& f : m_folders) {
            if (f.id == m_newItemFolderId) {
                previewFolder = f.name;
                break;
            }
        }
    }

    if (ImGui::BeginCombo("##FolderSelectCombo", previewFolder.c_str())) {
        if (ImGui::Selectable(I18N().allItems, m_newItemFolderId == 0)) {
            m_newItemFolderId = 0;
        }
        for (const auto& f : m_folders) {
            if (ImGui::Selectable(f.name.c_str(), m_newItemFolderId == f.id)) {
                m_newItemFolderId = f.id;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::Checkbox(I18N().itemMaskCheckbox, &m_newItemMasked);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float halfBtnW = (modalW - 40.0f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.47f, 0.83f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 0.92f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    if (ImGui::Button(I18N().btnSave, ImVec2(halfBtnW, 32))) {
        if (m_newItemContent[0] != '\0') {
            Database::Instance().AddCustomItem(m_newItemContent, m_newItemFolderId, m_newItemMasked ? 1 : 0);
            ReloadData();
            m_showCreateItemModal = false;
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    if (ImGui::Button(I18N().btnCancel, ImVec2(halfBtnW, 32))) {
        m_showCreateItemModal = false;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
