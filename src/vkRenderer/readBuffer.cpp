#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional>
#include <vector>
#include <array>

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "readBuffer.h"

using namespace vkDB;

//创建图像对象并分配显存
void vkDB::createImage(
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
    VkPhysicalDevice physicalDevice) {
    VkImageCreateInfo imageInfo{};
    //1-创建图像对象，只定义，不占显存，描述vkImage的状态信息：imageType/extent/mipmap等信息
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;//mipmap层级
    imageInfo.arrayLayers = 1;//是否是纹理数组或者3d纹理
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = numSamples;//每个像素的采样数

    if (vkCreateImage(logicDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    //2-根据上述描述查询其显存需求，并分配图像对应的显存
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vkDB::findMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice);

    if (vkAllocateMemory(logicDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(logicDevice, image, imageMemory, 0);
}

VkImageView vkDB::createImageView(
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags,
    uint32_t mipLevels, 
    VkDevice logicDevice)
{

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//1D/2D/3D/立方图纹理？
    viewInfo.format = format;
    //这块需要解释一下
    //subresourceRange字段描述了图像的用途
    viewInfo.subresourceRange.aspectMask = aspectFlags;//需要访问图像的哪个部分：颜色/深度/模板等

    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;

    viewInfo.subresourceRange.baseArrayLayer = 0;//是否是纹理数组或者立方体问题，0表示一级普通纹理
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(logicDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}

//查询内存类型
uint32_t vkDB::findMemoryType(
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties,
    VkPhysicalDevice physicalDevice) {
    //VkMemoryType    memoryTypes[VK_MAX_MEMORY_TYPES];//内存类型
    //VkMemoryHeap    memoryHeaps[VK_MAX_MEMORY_HEAPS];//类似于专用显存 (VRAM) 和用于显存耗尽时的 RAM 交换空间
    //查询有关可用内存类型的信息 
    VkPhysicalDeviceMemoryProperties memProperties;
    // 获取物理设备（显卡）的内存属性
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    //properties指定内存的特殊功能
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        //适用于缓冲区本身的内存类型
        // 1. typeFilter & (1 << i) : 检查第 i 种内存类型是否被允许用于我们的缓冲区（按位与测试）
        // 2. propertyFlags & properties : 检查该内存类型是否具备我们要求的所有特性（如对 CPU 可见、内存连贯等）
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

//模板格式？
bool vkDB::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void vkDB::transitionImageLayout(
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    uint32_t mipLevels,
    VkCommandBuffer commandBuffer)
{
    //VkCommandBuffer commandBuffer = this->beginSingleTimeCommands();
    //流水线屏障通常用于：1-同步资源访问，例如确保在读取缓冲区之前完成写入操作，2-转换图像布局，3-转换队列组所有权，当使用独占模式时（VK_SHARING_MODE_EXCLUSIVE）
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    //忽略队列组--如果是队列族所有权转换，那将是队列族索引
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    //指定受影响的图像及其具体部分，目前的图像不是数组，也没有 mipmapping 层级，因此只指定了一个层级和图层
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //mipmap？
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    //imageArray？
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;


    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }


    /**
     * sourceStage (源管线阶段):
     * 作用： 定义了屏障的等待条件。
     * 通俗解释： 告诉 GPU：“在执行屏障（包括图像布局转换）之前，你必须等待之前提交的命令执行完 sourceStage 指定的阶段。”
    */

    /**
    * destinationStage (目标管线阶段):
    * 作用： 定义了屏障的阻塞对象。
    * 通俗解释： 告诉 GPU：“在屏障（以及图像布局转换）完全结束之前，后续提交的命令绝对不允许进入 destinationStage 指定的阶段及之后的阶段。”
    */
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    //屏障主要用于同步，因此必须指定哪些涉及资源的操作必须在屏障之前执行（srcAccessMask），以及哪些涉及资源的操作必须等待屏障执行（dstAccessMask）
    //barrier.srcAccessMask:表示sourceStage 阶段中已经做了什么类型的内存读写操作，    
    //barrier.dstAccessMask:表示进入 destinationStage 阶段后，将会做什么类型的内存读写操作？
    barrier.srcAccessMask = 0; // TODO
    barrier.dstAccessMask = 0; // TODO

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//最早管线阶段
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;//管线传输阶段
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;//原布局是VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL（即被写入目标），所以转换之前要保证被写入完成
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//最早管线阶段
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }


    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage /* TODO */,//屏障之前的操作在哪个管线执行 
        destinationStage /* TODO */,//屏障之后的操作将在哪个管线执行
        0,
        //屏障类型
        0, nullptr,//内存屏障
        0, nullptr,//缓冲区内存屏障
        1, &barrier//图像内存屏障
    );

    //todo:
   //this->endSingleTimeCommands(commandBuffer);
}
