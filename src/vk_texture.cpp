#include "vk_texture.h"
#include "imgui_impl_vulkan.h"
#include "stb_image.h"

#include <unordered_map>
#include <vector>

namespace vk_texture {

struct CachedTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    int width = 0;
    int height = 0;
};

static VkDevice g_Device = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkQueue g_Queue = VK_NULL_HANDLE;
static uint32_t g_QueueFamily = 0;
static VkCommandPool g_CommandPool = VK_NULL_HANDLE;

static std::unordered_map<std::string, CachedTexture> g_Cache;

static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamily) {
    g_Device = device;
    g_PhysicalDevice = physicalDevice;
    g_Queue = queue;
    g_QueueFamily = queueFamily;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = g_QueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(g_Device, &poolInfo, nullptr, &g_CommandPool);
}

void Shutdown() {
    for (auto& pair : g_Cache) {
        CachedTexture& tex = pair.second;
        if (tex.descriptorSet) {
            ImGui_ImplVulkan_RemoveTexture(tex.descriptorSet);
        }
        if (tex.view) {
            vkDestroyImageView(g_Device, tex.view, nullptr);
        }
        if (tex.image) {
            vkDestroyImage(g_Device, tex.image, nullptr);
        }
        if (tex.memory) {
            vkFreeMemory(g_Device, tex.memory, nullptr);
        }
    }
    g_Cache.clear();

    if (g_CommandPool) {
        vkDestroyCommandPool(g_Device, g_CommandPool, nullptr);
        g_CommandPool = VK_NULL_HANDLE;
    }
}

static VkCommandBuffer BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = g_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(g_Device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(g_Queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_Queue);

    vkFreeCommandBuffers(g_Device, g_CommandPool, 1, &commandBuffer);
}

ImTextureID GetOrCreateTexture(const std::string& imagePath, int& outWidth, int& outHeight) {
    auto it = g_Cache.find(imagePath);
    if (it != g_Cache.end()) {
        outWidth = it->second.width;
        outHeight = it->second.height;
        return (ImTextureID)it->second.descriptorSet;
    }

    if (!g_Device || !g_CommandPool) {
        return 0;
    }

    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load(imagePath.c_str(), &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) stbi_image_free(pixels);
        return 0;
    }

    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(g_Device, &bufferInfo, nullptr, &stagingBuffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(g_Device, stagingBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(g_Device, &allocInfo, nullptr, &stagingBufferMemory);
        vkBindBufferMemory(g_Device, stagingBuffer, stagingBufferMemory, 0);

        void* data;
        vkMapMemory(g_Device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, (size_t)imageSize);
        vkUnmapMemory(g_Device, stagingBufferMemory);
    }
    stbi_image_free(pixels);

    CachedTexture tex;
    tex.width = width;
    tex.height = height;

    {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vkCreateImage(g_Device, &imageInfo, nullptr, &tex.image);

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(g_Device, tex.image, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(g_Device, &allocInfo, nullptr, &tex.memory);
        vkBindImageMemory(g_Device, tex.image, tex.memory, 0);
    }

    {
        VkCommandBuffer cmd = BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = tex.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };
        vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndSingleTimeCommands(cmd);
    }

    vkDestroyBuffer(g_Device, stagingBuffer, nullptr);
    vkFreeMemory(g_Device, stagingBufferMemory, nullptr);

    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = tex.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(g_Device, &viewInfo, nullptr, &tex.view);
    }

    tex.descriptorSet = ImGui_ImplVulkan_AddTexture(tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    g_Cache[imagePath] = tex;
    outWidth = width;
    outHeight = height;

    return (ImTextureID)tex.descriptorSet;
}

}
