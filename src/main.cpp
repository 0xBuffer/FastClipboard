#include "imgui.h"
#include "imgui_impl_win32.h"
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include "imgui_impl_vulkan.h"

#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "clipboard.h"
#include "hotkey.h"
#include "installer.h"
#include "database.h"
#include "vk_texture.h"
#include "i18n.h"

static VkAllocationCallbacks*   g_Allocator = nullptr;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

HWND g_hWnd = NULL;
static HWND g_TargetWnd = NULL;
static NOTIFYICONDATAW g_TrayNid = {};

static const int WINDOW_WIDTH = 380;
static const int WINDOW_HEIGHT = 520;

static void check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) return;
    fprintf(stderr, "[vulkan] Note: VkResult = %d\n", err);
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension) {
    for (const VkExtensionProperties& p : properties) {
        if (strcmp(p.extensionName, extension) == 0)
            return true;
    }
    return false;
}

static void SetupVulkan(ImVector<const char*> instance_extensions) {
    VkResult err;

    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
        check_vk_result(err);

        if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
        check_vk_result(err);
    }

    g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);

    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = 1;
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;

        err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1024 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 2048;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height) {
    wd->Surface = surface;
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat,
        (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);

    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, &present_modes[0], 1);

    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
}

static void CleanupVulkan() {
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd) {
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, wd, g_Allocator);
    vkDestroySurfaceKHR(g_Instance, wd->Surface, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data) {
    VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_SURFACE_LOST_KHR) {
        g_SwapChainRebuild = true;
        return;
    }
    if (err != VK_SUCCESS) {
        g_SwapChainRebuild = true;
        return;
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
        if (err != VK_SUCCESS) {
            g_SwapChainRebuild = true;
            return;
        }
        err = vkResetFences(g_Device, 1, &fd->Fence);
        if (err != VK_SUCCESS) {
            g_SwapChainRebuild = true;
            return;
        }
    }
    {
        err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
        if (err != VK_SUCCESS) {
            g_SwapChainRebuild = true;
            return;
        }
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        if (err != VK_SUCCESS) {
            g_SwapChainRebuild = true;
            return;
        }
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        if (err != VK_SUCCESS) {
            g_SwapChainRebuild = true;
            return;
        }
        err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
        if (err != VK_SUCCESS) {
            g_SwapChainRebuild = true;
            return;
        }
    }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd) {
    if (g_SwapChainRebuild)
        return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_SURFACE_LOST_KHR) {
        g_SwapChainRebuild = true;
        return;
    }
    if (err != VK_SUCCESS) {
        g_SwapChainRebuild = true;
        return;
    }
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void ShowClipboardWindow() {
    hotkey::EnsureHookActive();
    g_SwapChainRebuild = true;

    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_RWIN, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LMENU, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_RMENU, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);

    POINT pt = { 0, 0 };
    hotkey::DetectTargetAndPosition(g_TargetWnd, pt, WINDOW_WIDTH, WINDOW_HEIGHT);

    SetWindowPos(g_hWnd, HWND_TOPMOST, pt.x, pt.y, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_SHOWWINDOW);
    SetForegroundWindow(g_hWnd);
    SetActiveWindow(g_hWnd);

    App::Instance().OnShow(g_TargetWnd);
}

static void HideClipboardWindow() {
    ShowWindow(g_hWnd, SW_HIDE);
    App::Instance().OnHide();
}

static void ShowTrayContextMenu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, 1001, I18N().trayOpen);
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, 1002, I18N().trayExit);

    SetForegroundWindow(g_hWnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_hWnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 1001) {
        ShowClipboardWindow();
    } else if (cmd == 1002) {
        PostQuitMessage(0);
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_Device != VK_NULL_HANDLE && wParam != SIZE_MINIMIZED) {
            int fb_width = (UINT)LOWORD(lParam);
            int fb_height = (UINT)HIWORD(lParam);
            if (fb_width > 0 && fb_height > 0) {
                HRGN hRgn = CreateRoundRectRgn(0, 0, fb_width, fb_height, 24, 24);
                SetWindowRgn(hWnd, hRgn, TRUE);
            }
            if (fb_width > 0 && fb_height > 0 &&
                (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height)) {
                ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(
                    g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData,
                    g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount, 0);
                g_MainWindowData.FrameIndex = 0;
                g_SwapChainRebuild = false;
            }
        }
        return 0;

    case WM_CLIPBOARDUPDATE:
        clipboard::OnClipboardUpdate(hWnd);
        App::Instance().ReloadData();
        return 0;

    case WM_FASTCLIPBOARD_TOGGLE:
        if (IsWindowVisible(hWnd)) {
            HideClipboardWindow();
        } else {
            ShowClipboardWindow();
        }
        return 0;

    case WM_FASTCLIPBOARD_TRAY:
        if (lParam == WM_LBUTTONUP) {
            if (IsWindowVisible(hWnd)) {
                HideClipboardWindow();
            } else {
                ShowClipboardWindow();
            }
        } else if (lParam == WM_RBUTTONUP) {
            ShowTrayContextMenu();
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !App::Instance().IsFirstRun()) {
            HideClipboardWindow();
        }
        return 0;

    case WM_TIMER:
        if (wParam == 1001) {
            hotkey::EnsureHookActive();
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, 1001);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

ImFont* g_FontDefault = nullptr;
ImFont* g_FontSmall = nullptr;

int main(int, char**) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"FastClipboardSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existingWnd = FindWindowW(L"FastClipboardClass", nullptr);
        if (existingWnd) {
            PostMessage(existingWnd, WM_FASTCLIPBOARD_TOGGLE, 0, 0);
        }
        return 0;
    }

    RegisterApplicationRestart(L"", 0);

    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"FastClipboardClass", nullptr
    };
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int initX = (screenW - WINDOW_WIDTH) / 2;
    int initY = (screenH - WINDOW_HEIGHT) / 2;

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"FastClipboard",
        WS_POPUP,
        initX, initY, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    SetTimer(g_hWnd, 1001, 30000, nullptr);

    #ifndef DWMWA_WINDOW_CORNER_PREFERENCE
    #define DWMWA_WINDOW_CORNER_PREFERENCE 33
    #endif
    int roundPref = 2;
    DwmSetWindowAttribute(g_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &roundPref, sizeof(roundPref));

    HRGN hRgn = CreateRoundRectRgn(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 24, 24);
    SetWindowRgn(g_hWnd, hRgn, TRUE);

    ImVector<const char*> extensions;
    extensions.push_back("VK_KHR_surface");
    extensions.push_back("VK_KHR_win32_surface");
    SetupVulkan(extensions);

    VkSurfaceKHR surface;
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hwnd = g_hWnd;
    surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
    VkResult err = vkCreateWin32SurfaceKHR(g_Instance, &surfaceCreateInfo, nullptr, &surface);
    check_vk_result(err);

    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, WINDOW_WIDTH, WINDOW_HEIGHT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    static const ImWchar glyph_ranges[] = {
        0x0020, 0x00FF,
        0x0100, 0x024F,
        0x0400, 0x052F,
        0x2DE0, 0x2DFF,
        0xA640, 0xA69F,
        0x2000, 0x206F,
        0x20A0, 0x20CF,
        0x2100, 0x214F,
        0x2190, 0x22FF,
        0x25A0, 0x26FF,
        0,
    };

    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = false;
    fontCfg.RasterizerMultiply = 1.25f;

    const char* fontSbPath = "C:\\Windows\\Fonts\\seguisb.ttf";
    const char* fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* chosenFont = (GetFileAttributesA(fontSbPath) != INVALID_FILE_ATTRIBUTES) ? fontSbPath : fontPath;

    if (GetFileAttributesA(chosenFont) != INVALID_FILE_ATTRIBUTES) {
        g_FontDefault = io.Fonts->AddFontFromFileTTF(chosenFont, 15.5f, &fontCfg, glyph_ranges);
        g_FontSmall = io.Fonts->AddFontFromFileTTF(chosenFont, 12.0f, &fontCfg, glyph_ranges);
    } else {
        g_FontDefault = io.Fonts->AddFontDefault();
        g_FontSmall = g_FontDefault;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.125f, 0.125f, 0.125f, 0.98f);
    style.Colors[ImGuiCol_PopupBg]              = ImVec4(0.170f, 0.170f, 0.170f, 0.99f);
    style.Colors[ImGuiCol_Border]               = ImVec4(0.220f, 0.220f, 0.220f, 0.70f);
    style.Colors[ImGuiCol_FrameBg]              = ImVec4(0.165f, 0.165f, 0.165f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.200f, 0.200f, 0.200f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);
    style.Colors[ImGuiCol_Button]               = ImVec4(0.180f, 0.180f, 0.180f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.230f, 0.230f, 0.230f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.150f, 0.150f, 0.150f, 1.0f);
    style.Colors[ImGuiCol_Header]               = ImVec4(0.200f, 0.200f, 0.200f, 0.9f);
    style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.250f, 0.250f, 0.250f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.000f, 0.470f, 0.830f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.100f, 0.100f, 0.100f, 0.0f);
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.300f, 0.300f, 0.300f, 0.5f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.400f, 0.400f, 0.400f, 0.8f);
    style.Colors[ImGuiCol_Text]                 = ImVec4(1.000f, 1.000f, 1.000f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled]         = ImVec4(0.620f, 0.620f, 0.620f, 1.0f);

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    vk_texture::Init(g_Device, g_PhysicalDevice, g_Queue, g_QueueFamily);

    App::Instance().Init();
    clipboard::Init(g_hWnd);
    hotkey::InstallHook(g_hWnd);

    g_TrayNid.cbSize = sizeof(NOTIFYICONDATAW);
    g_TrayNid.hWnd = g_hWnd;
    g_TrayNid.uID = 1;
    g_TrayNid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_TrayNid.uCallbackMessage = WM_FASTCLIPBOARD_TRAY;
    g_TrayNid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_TrayNid.szTip, I18N().trayTip);
    Shell_NotifyIconW(NIM_ADD, &g_TrayNid);

    if (App::Instance().IsFirstRun()) {
        ShowWindow(g_hWnd, SW_SHOW);
        UpdateWindow(g_hWnd);
    } else {
        ShowWindow(g_hWnd, SW_HIDE);
    }

    bool done = false;
    while (!done) {
        MSG msg;
        if (!IsWindowVisible(g_hWnd)) {
            if (GetMessage(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    done = true;
                }
            } else {
                done = true;
            }
            continue;
        }

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) break;

        if (App::Instance().ShouldClose()) {
            HideClipboardWindow();
            continue;
        }

        if (g_SwapChainRebuild) {
            int fb_width = WINDOW_WIDTH;
            int fb_height = WINDOW_HEIGHT;
            RECT rect;
            if (GetClientRect(g_hWnd, &rect)) {
                fb_width = rect.right - rect.left;
                fb_height = rect.bottom - rect.top;
            }
            if (fb_width > 0 && fb_height > 0) {
                ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(
                    g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData,
                    g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount, 0);
                g_MainWindowData.FrameIndex = 0;
                g_SwapChainRebuild = false;
            }
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        App::Instance().Render();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            wd->ClearValue.color.float32[0] = 0.09f;
            wd->ClearValue.color.float32[1] = 0.10f;
            wd->ClearValue.color.float32[2] = 0.13f;
            wd->ClearValue.color.float32[3] = 1.0f;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }

        Sleep(10);
    }

    Shell_NotifyIconW(NIM_DELETE, &g_TrayNid);
    hotkey::UninstallHook();
    RemoveClipboardFormatListener(g_hWnd);

    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);

    vk_texture::Shutdown();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow(&g_MainWindowData);
    CleanupVulkan();

    DestroyWindow(g_hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CloseHandle(hMutex);

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(0, nullptr);
}
