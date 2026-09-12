// VulkanBuffer.h (设计蓝图，请您参考编写)
#pragma once

#include <vulkan/vulkan.h>
#include <memory>
// 依赖基础环境和命令管理器 (用于内存拷贝)
class VulkanContext;
class VulkanCommandManager;

class VulkanBuffer {
public:
    // 构造函数：分配缓冲区并分配底层显存
    // deviceSize: 数据大小
    // usageFlags: 用途 (例如 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    // memoryPropertyFlags: 显存特性 (例如 CPU可见、GPU专享等)
    VulkanBuffer(
        std::shared_ptr<VulkanContext> context,
        std::shared_ptr<VulkanCommandManager> vkCommandManager,
        VkDeviceSize deviceSize,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags
    );

    // 析构函数：释放 _buffer 和 _bufferMemory
    ~VulkanBuffer();

    // 禁用拷贝语义 (避免显存被双重释放)
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // ----- 获取底层资源句柄 -----
    VkBuffer getBuffer() const { return _vkBuffer; }
    VkDeviceMemory getBufferMemory() const { return _vkBufferMemory; }
    VkDeviceSize getBufferSize() const { return _bufferSize; }

    // ----- CPU 读写接口 (仅在创建时带有 HOST_VISIBLE 属性才有效) -----

    // 映射内存：拿到一个可以给 CPU 读写的 void* 指针
    void map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    // 写入数据：将外部数据拷贝进 mapped 区域
    void setBufferData(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    // 解除映射
    void unmap();

    // ----- 暂存缓冲工具 (Staging Buffer) -----
    // 用于将当前缓冲区的数据，利用命令缓冲高效地拷贝到目标 GPU 缓冲区中
    void copyToBuffer(
        VkBuffer dstBuffer,
        VkDeviceSize size = VK_WHOLE_SIZE);

    void copyToImage(
        VkImage image,
        uint32_t width,
        uint32_t height);
private:
    std::shared_ptr<VulkanContext> _vkContext;
    std::shared_ptr<VulkanCommandManager> _vkCommandManager;

    VkBuffer _vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory _vkBufferMemory = VK_NULL_HANDLE;
    // 映射后的内存指针，避免频繁 Map/Unmap
    void* _mappedData = nullptr;

    VkDeviceSize _bufferSize = 0;
    VkBufferUsageFlags _usageFlags;
    VkMemoryPropertyFlags _memoryPropertyFlags;

    void createBuffer();
};