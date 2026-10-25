#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <deque>
class VulkanContext;

// ==========================================================
// 类 1：动态描述符池管理器 (VulkanDescriptorManager)
// 职责：安全且可扩容地分配 VkDescriptorSet
// ==========================================================
class VulkanDescriptorManager {
public:
    VulkanDescriptorManager(std::shared_ptr<VulkanContext> context);
    ~VulkanDescriptorManager();

    // 禁用拷贝
    VulkanDescriptorManager(const VulkanDescriptorManager&) = delete;
    VulkanDescriptorManager& operator=(const VulkanDescriptorManager&) = delete;

    // 清理所有已分配的资源（通常在场景重置时调用）
    void resetPools();

    // 核心功能：尝试分配一个描述符集。如果当前池满了，自动创建新池。
    bool allocate(VkDescriptorSet& set, VkDescriptorSetLayout layout);

private:
    std::shared_ptr<VulkanContext> _vkContext;

    // 当前正在使用的池子
    VkDescriptorPool _currentPool = VK_NULL_HANDLE;
    // 满掉的旧池子缓存，留作最后统一销毁（使用完毕的旧池子）
    std::vector<VkDescriptorPool> _usedPools;
    // 未来如果需要复用被 reset 的池子，可放这里（空闲的可使用的池子）
    std::vector<VkDescriptorPool> _freePools;

    // 获取一个可用的池子（从 free 队列拿或者新建）
    VkDescriptorPool grabPool();

    // 私有辅助：向显卡申请一个具有标准大小比例的新 Pool
    VkDescriptorPool createPool();
};


// ==========================================================
// 类 2：描述符集构建器 (VulkanDescriptorBuilder)
// 职责：提供流式接口，快速打包 Layout 蓝图并连接物理资源数据
// ==========================================================
class VulkanDescriptorBuilder {
public:
    // 构造时需要知道找谁去申请内存池 (Allocator)
    VulkanDescriptorBuilder(
        std::shared_ptr<VulkanContext> context,
        std::shared_ptr<VulkanDescriptorManager> allocator);

    ~VulkanDescriptorBuilder();

    // ---------------- 流式绑定接口 ----------------

    // 绑定一个 Buffer 资源（例如 UBO 或 SSBO）
    void bindBuffer(
        uint32_t binding,//几号描述符
        VkDescriptorType type,//资源类型（ubo/ssbo/sampler）
        VkShaderStageFlags stageFlags,//shader使用阶段
        const VkDescriptorBufferInfo* bufferInfo//对应的真实buffer信息
    );

    // 绑定一个 Image 资源（例如 纹理贴图）
    void bindImage(
        uint32_t binding,
        VkDescriptorType type,
        VkShaderStageFlags stageFlags,
        const VkDescriptorImageInfo* imageInfo);

    // ---------------- 最终构建 ----------------

    // 终极武器：传入两个指针，它将为您生成 Layout 图纸和可用的 Set 插头！
    // 返回 bool 表示是否构建成功。
    bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
    void reset();

private:
    std::shared_ptr<VulkanContext> _vkContext;
    std::shared_ptr<VulkanDescriptorManager> _allocator;

    // 用于组装 Layout 图纸的临时数据（描述符集所有的说明书）
    std::vector<VkDescriptorSetLayoutBinding> _bindings;

    // 用于组装写入资源 (Update) 的临时数据
    // 注意：VkWriteDescriptorSet 结构体非常娇贵，内部包含了指针，
    // 在存入 std::vector 时需要特别小心浅拷贝问题！
    std::vector<VkWriteDescriptorSet> _writes;

    std::deque<VkDescriptorBufferInfo> _bufferInfos;
    std::deque<VkDescriptorImageInfo> _imageInfos;



    void bindData(
        uint32_t binding,//几号描述符
        VkDescriptorType type,//资源类型（ubo/ssbo/sampler）
        VkShaderStageFlags stageFlags,//shader使用阶段
        const VkDescriptorBufferInfo* bufferInfo,//对应的真实buffer信息
        const VkDescriptorImageInfo* imageInfo);
};