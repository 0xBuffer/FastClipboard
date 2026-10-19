# FastClipboard ⚡

[English](#english) | [Русский](#русский)

---

<a name="english"></a>
## English

**FastClipboard** is a fast, lightweight Windows clipboard manager built with modern C++ (C++17), hardware-accelerated **Dear ImGui (Vulkan)**, and persistent storage via **SQLite3**.

### ✨ Features

- **Fluent / UWP Dark Matte Interface**: Frameless design with smooth rounded corners, native typography (Segoe UI Semibold), and dark matte styling.
- **Global Hotkeys**:
  - `Win + V` (natively intercepts and replaces default Windows clipboard history without triggering the Start Menu).
  - `Left Alt` (quick standalone tap or `Left Alt + V`).
- **Smart Target & Caret Positioning**:
  - Detects active caret position and places the window directly beneath text input fields.
  - Automatically falls back to mouse cursor position with intelligent monitor boundary flipping.
- **Data Persistence**:
  - Automatically stores texts and clipboard images in `%APPDATA%\FastClipboard\clipboard.db`.
  - Zero loss on reboot or logoff.
- **Masked / Hidden Pastes**:
  - Mask sensitive pastes (passwords, tokens, API keys) into previews like `pa****37`.
  - Content remains intact and pastes full unmasked text on click.
- **Manual Paste Creation**:
  - Add custom pastes directly into root or any specific folder using the `+` button in the top bar.
- **Folders & Categories**:
  - Organize clips into folders via a smooth slide-out side drawer.
  - Full search filter in real-time.
- **Smooth Animations**:
  - Smooth drawer slide-in / slide-out animation with fading backdrop.
  - Smooth momentum list scrolling without ugly scrollbar tracks.
- **Zero Idle CPU**:
  - Waits efficiently on Windows message loop when hidden (0.0% CPU usage).
- **Automatic Localization**:
  - Automatically detects Windows UI language (Russian & English supported).

### 🛠 Building

#### Prerequisites
- **CMake** 3.20+
- **Ninja** or MinGW Makefiles
- **GCC / MinGW-w64** (C++17) or MSVC
- **Vulkan SDK** 1.3+

#### Build Command
```powershell
cmake -B build -G "Ninja" -DCMAKE_C_COMPILER=gcc.exe -DCMAKE_CXX_COMPILER=g++.exe
cmake --build build
```

Binary is output to `build/FastClipboard.exe`.

---

<a name="русский"></a>
## Русский

**FastClipboard** — быстрый и легковесный менеджер буфера обмена для Windows на C++ с аппаратным ускорением через **Dear ImGui (Vulkan)** и вечным хранением в **SQLite3**.

### ✨ Возможности

- **Интерфейс в стиле Windows 11 Fluent / UWP**: Безрамочный дизайн со скругленными углами, фирменным полужирным шрифтом (Segoe UI Semibold) и матовым оформлением.
- **Горячие клавиши вызова**:
  - `Win + V` (перехват стандартного клипборда без открытия меню Пуск при отпускании Win).
  - `Левый Alt` (быстрое одиночное нажатие или `Левый Alt + V`).
- **Умное позиционирование**:
  - Автоматически находит текстовую каретку / поле ввода и открывается под ним.
  - При отсутствии поля ввода открывается прямо у курсора мыши с защитой от вылета за экран.
- **Вечное локальное хранение**:
  - Все скопированные тексты и изображения сохраняются в `%APPDATA%\FastClipboard\clipboard.db`.
- **Скрытые записи (маскирование паролей)**:
  - Возможность скрыть любую запись: вместо `password1337` отображается `pa****37`.
  - При клике по карточке вставляется настоящий полный пароль.
- **Ручное добавление записей**:
  - Создание заметок и паст как в общую категорию, так и в любую созданную папку по кнопке `+`.
- **Папки и категории**:
  - Удобная боковая панель с плавной анимацией выезда.
  - Поиск в реальном времени.
- **Плавные анимации**:
  - Анимация открытия бокового меню и затемнения фона.
  - Плавная инерционная прокрутка колесиком мыши без громоздкой полосы прокрутки.
- **0.0% CPU в фоне**:
  - В скрытом режиме процесс не нагружает систему и ожидает системных событий.
- **Двуязычный интерфейс**:
  - Автоматически выставляет русский или английский язык в зависимости от языка Windows.

### 🛠 Сборка
```powershell
cmake -B build -G "Ninja" -DCMAKE_C_COMPILER=gcc.exe -DCMAKE_CXX_COMPILER=g++.exe
cmake --build build
```
Исполняемый файл: `build/FastClipboard.exe`.
