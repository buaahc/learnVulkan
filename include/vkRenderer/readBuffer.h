#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional>
#include <vector>
#include <array>



#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "vulkanContext.h" 

namespace vkDB
{

    //创建图像对象并分配显存
    void createImage(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkSampleCountFlagBits numSamples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory,
        VkDevice logicDevice,
        VkPhysicalDevice physicalDevice);

    VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags,
        uint32_t mipLevels,
        VkDevice logicDevice);

    //查询内存类型
    uint32_t findMemoryType(
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties,
        VkPhysicalDevice physicalDevice);


    void transitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t mipLevels,
        VkCommandBuffer commandBuffer);

    //模板格式？
    bool hasStencilComponent(VkFormat format);
}