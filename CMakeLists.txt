cmake_minimum_required(VERSION 3.20)
project(FastClipboard LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Vulkan
if(DEFINED ENV{VULKAN_SDK})
    set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
    set(Vulkan_LIBRARY "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
else()
    set(Vulkan_INCLUDE_DIR "C:/VulkanSDK/1.4.363.0/Include")
    set(Vulkan_LIBRARY "C:/VulkanSDK/1.4.363.0/Lib/vulkan-1.lib")
endif()

message(STATUS "Vulkan Include: ${Vulkan_INCLUDE_DIR}")
message(STATUS "Vulkan Library: ${Vulkan_LIBRARY}")

# ImGui sources
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# SQLite sources
set(SQLITE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/sqlite/sqlite-amalgamation-3470200)
set(SQLITE_SOURCES
    ${SQLITE_DIR}/sqlite3.c
)

# STB include dir
set(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/stb)

include_directories(
    ${Vulkan_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${SQLITE_DIR}
    ${STB_DIR}
    src
)

# Compile SQLite without extra warning noise
set_source_files_properties(${SQLITE_SOURCES} PROPERTIES COMPILE_FLAGS "-DSQLITE_ENABLE_RTREE=1 -w")

add_library(imgui STATIC ${IMGUI_SOURCES})
target_compile_definitions(imgui PUBLIC VK_USE_PLATFORM_WIN32_KHR)
target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIR} ${IMGUI_DIR} ${IMGUI_DIR}/backends)

add_library(sqlite STATIC ${SQLITE_SOURCES})
target_include_directories(sqlite PUBLIC ${SQLITE_DIR})

set(APP_SOURCES
    src/main.cpp
    src/app.cpp
    src/clipboard.cpp
    src/database.cpp
    src/hotkey.cpp
    src/installer.cpp
    src/vk_texture.cpp
    res/resource.rc
)

add_executable(FastClipboard WIN32 ${APP_SOURCES})
target_link_libraries(FastClipboard PRIVATE
    imgui
    sqlite
    ${Vulkan_LIBRARY}
    dwmapi
    gdi32
    user32
    shell32
    ole32
    oleaut32
    uuid
    shlwapi
)
