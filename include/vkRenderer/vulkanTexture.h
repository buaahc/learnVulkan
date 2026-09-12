// vulkanTexture.h (设计蓝图)
#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>

// 依赖的基础模块
class VulkanContext;
class VulkanCommandManager;

class VulkanTexture {
public:
    // 构造函数：负责读取图片文件，创建 Image，并通过 staging buffer 拷贝数据
    VulkanTexture(
        std::shared_ptr<VulkanContext> context,
        std::shared_ptr<VulkanCommandManager> commandManager,
        const std::string& filePath
    );

    ~VulkanTexture();

    // 禁用拷贝语义
    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    // ----- 获取供描述符集 (Descriptor Set) 绑定的句柄 -----
    VkImageView getImageView() const { return _vkImageView; }
    VkSampler getSampler() const { return _vkSampler; }

    // (可选) 获取底层 Image
    VkImage getImage() const { return _vkImage; }

private:
    std::shared_ptr<VulkanContext> _vkContext = nullptr;
    std::shared_ptr<VulkanCommandManager> _vkCommandManager = nullptr;
    std::string _imageFilePath = "";

    VkImage _vkImage = VK_NULL_HANDLE;
    VkDeviceMemory _vkImageMemory = VK_NULL_HANDLE;
    VkImageView _vkImageView = VK_NULL_HANDLE;
    VkSampler _vkSampler = VK_NULL_HANDLE;

    uint32_t _mipLevels = 1;
    int _texWidth = 0;
    int _texHeight = 0;
    int _texChannels = 0;

    // ----- 私有初始化步骤 -----
    void createTextureImage();
    void createTextureSampler();

    void generateMipmaps(
        VkFormat imageFormat,
        int32_t texWidth,
        int32_t texHeight,
        uint32_t mipLevels);
};