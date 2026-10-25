#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "imgui.h"

namespace vk_texture {

void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamily);
void NewFrame();
void Shutdown();
ImTextureID GetOrCreateTexture(const std::string& imagePath, int& outWidth, int& outHeight);

}
