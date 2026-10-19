#pragma once
#include <windows.h>
#include <string>

enum class Language {
    Russian,
    English
};

inline Language DetectSystemLanguage() {
    LANGID langId = GetUserDefaultUILanguage();
    WORD pri = PRIMARYLANGID(langId);
    if (pri == LANG_RUSSIAN || pri == LANG_BELARUSIAN || pri == LANG_UKRAINIAN) {
        return Language::Russian;
    }
    return Language::English;
}

struct LocStrings {
    const char* setupTitle;
    const char* setupWelcome;
    const char* setupDesc;
    const char* setupAutostart;
    const char* setupHotkeyHint;
    const char* setupStartBtn;

    const char* searchHint;
    const char* searchAll;
    const char* tooltipFolders;
    const char* tooltipNewItem;
    const char* tooltipClose;

    const char* folderPrefix;
    const char* showAll;
    const char* emptyTitle;
    const char* emptyDesc;

    const char* actionsTitle;
    const char* actionDelete;
    const char* actionMoveToFolder;
    const char* actionNoFolder;
    const char* actionMask;
    const char* actionUnmask;

    const char* foldersTitle;
    const char* btnCreateFolder;
    const char* allItems;
    const char* modalCreateFolderTitle;
    const char* folderNamePlaceholder;
    const char* btnCreate;
    const char* btnCancel;

    const char* modalCreateItemTitle;
    const char* itemContentPlaceholder;
    const char* itemFolderLabel;
    const char* itemMaskCheckbox;
    const char* btnSave;

    const wchar_t* trayOpen;
    const wchar_t* trayExit;
    const wchar_t* trayTip;
};

inline const LocStrings& I18N() {
    static const LocStrings ru = {
        "Первоначальная настройка FastClipboard",
        "Добро пожаловать в FastClipboard!",
        "Быстрый менеджер буфера обмена на C++ и Vulkan.",
        "Запускать вместе с Windows (автозагрузка)",
        "Горячие клавиши: Win+V или Левый Alt",
        "Начать работу",

        "Поиск в %s...",
        "Все элементы",
        "Папки и категории",
        "Создать запись",
        "Скрыть окно",

        "Папка: ",
        "Показать все",
        "Буфер обмена пуст",
        "Скопируйте текст или картинку (Ctrl+C)",

        "Действия с записью",
        "Удалить",
        "Переместить в папку",
        "Без папки",
        "Скрыть содержимое",
        "Показать содержимое",

        "Папки",
        "+ Создать папку",
        "Все элементы",
        "Создание новой папки",
        "Название папки...",
        "Создать",
        "Отмена",

        "Создать новую запись",
        "Введите текст записи...",
        "Папка для сохранения:",
        "Скрыть содержимое (пароль / секрет)",
        "Сохранить",

        L"Открыть FastClipboard (Win+V / Left Alt)",
        L"Выход",
        L"FastClipboard (Win+V / Left Alt)"
    };

    static const LocStrings en = {
        "FastClipboard Initial Setup",
        "Welcome to FastClipboard!",
        "Fast clipboard manager powered by C++ & Vulkan.",
        "Launch on Windows startup",
        "Hotkeys: Win+V or Left Alt",
        "Get Started",

        "Search in %s...",
        "All Items",
        "Folders & Categories",
        "New paste",
        "Hide window",

        "Folder: ",
        "Show all",
        "Clipboard is empty",
        "Copy text or image (Ctrl+C)",

        "Item Actions",
        "Delete",
        "Move to folder",
        "No folder",
        "Mask content",
        "Unmask content",

        "Folders",
        "+ New Folder",
        "All Items",
        "Create New Folder",
        "Folder name...",
        "Create",
        "Cancel",

        "Create New Paste",
        "Enter text content...",
        "Save to folder:",
        "Mask content (password / secret)",
        "Save",

        L"Open FastClipboard (Win+V / Left Alt)",
        L"Exit",
        L"FastClipboard (Win+V / Left Alt)"
    };

    static Language lang = DetectSystemLanguage();
    return (lang == Language::Russian) ? ru : en;
}
