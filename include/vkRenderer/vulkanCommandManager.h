// vkCommandManager.h (设计草案，请您手写)
#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

// 前置声明，避免循环包含
class VulkanContext;

class VulkanCommandManager {
public:
    // 构造时需要 Context 提供逻辑设备和图形队列的索引
    //explicit（明确的） 关键字用来禁止 C++ 编译器在背后偷偷做“隐式类型转换”   
    explicit VulkanCommandManager(std::shared_ptr<VulkanContext> context);

    // 禁用拷贝（避免 CommandPool 被意外复制释放）
    VulkanCommandManager(const VulkanCommandManager&) = delete;
    VulkanCommandManager& operator=(const VulkanCommandManager&) = delete;

    // ----- 核心功能 1：批量分配常规命令缓冲区 -----
    // 用于分配主渲染循环（每帧）使用的 Command Buffers
    std::vector<VkCommandBuffer> createCommandBuffers(uint32_t count, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // ----- 核心功能 2：单次指令提交 (Single Time Commands) -----
    // 用于初始化、资源拷贝、布局转换等瞬间完成的任务

    //开启一个临时命令缓冲区并进入录制状态    
    VkCommandBuffer beginSingleTimeCommands();

    // 结束录制，立即提交给图形队列，并阻塞等待它执行完毕，然后销毁它
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void cleanup();
private:
    std::shared_ptr<VulkanContext> _vkContext; // 弱耦合持有 Context

    VkCommandPool _commandPool = VK_NULL_HANDLE;

    //私有初始化 
    void createCommandPool();
};