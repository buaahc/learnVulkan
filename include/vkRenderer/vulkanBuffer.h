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
    VkBuffer getBuffer() const { return _buffer; }
    VkDeviceMemory getBufferMemory() const { return _bufferMemory; }
    VkDeviceSize getBufferSize() const { return _bufferSize; }

    // ----- CPU 读写接口 (仅在创建时带有 HOST_VISIBLE 属性才有效) -----

    // 映射内存：拿到一个可以给 CPU 读写的 void* 指针
    void map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    // 写入数据：将外部数据拷贝进 mapped 区域
    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    // 解除映射
    void unmap();

    // ----- 暂存缓冲工具 (Staging Buffer) -----
    // 用于将当前缓冲区的数据，利用命令缓冲高效地拷贝到目标 GPU 缓冲区中
    void copyBuffer(
        std::shared_ptr<VulkanCommandManager> commandManager,
        VkBuffer dstBuffer,
        VkDeviceSize size = VK_WHOLE_SIZE
    );

private:
    std::shared_ptr<VulkanContext> _vkContext;

    VkBuffer _buffer = VK_NULL_HANDLE;
    VkDeviceMemory _bufferMemory = VK_NULL_HANDLE;
    // 映射后的内存指针，避免频繁 Map/Unmap
    void* _mappedData = nullptr;

    VkDeviceSize _bufferSize = 0;
    VkBufferUsageFlags _usageFlags;
    VkMemoryPropertyFlags _memoryPropertyFlags;

    void createBuffer();
};