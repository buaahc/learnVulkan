#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class VulkanContext;
class VulkanBuffer;
class VulkanCommandManager;

// 从原 compute.cpp 搬运的粒子结构体
struct Particle {
    glm::vec2 _position;
    glm::vec2 _velocity;
    glm::vec4 _color;

    // 获取供 Graphics Pipeline 绑定的描述
     //==============================================================
    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class VulkanParticleSystem {
public:
    // 构造函数：初始化粒子数据，分配 SSBO 显存，并通过 staging buffer 送入 GPU
    VulkanParticleSystem(
        std::shared_ptr<VulkanContext> context,
        std::shared_ptr<VulkanCommandManager> commandManager,
        uint32_t particleCount,
        const uint32_t screenWidth,
        const uint32_t screenHeight,
        int maxFrameInFlight);

    ~VulkanParticleSystem() = default; // VulkanBuffer 的 RAII 会自动释放显存

    // 禁用拷贝语义
    VulkanParticleSystem(const VulkanParticleSystem&) = delete;
    VulkanParticleSystem& operator=(const VulkanParticleSystem&) = delete;

    // ----- 资源获取接口 (供 DescriptorManager 绑定 SSBO 时使用) -----
    // 用于 Compute Pipeline 绑定
    //VkDescriptorBufferInfo getSSBOBufferInfo(uint32_t frameIndex) const;

    // ----- 核心渲染接口 (供 Graphics Pipeline 使用) -----
    // 将当前帧对应的 SSBO 作为 Vertex Buffer 绑定到指令缓冲
    //==============================================================
    void bindBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame);

    // 发出绘制指令 (无索引绘制)
    void draw(VkCommandBuffer commandBuffer);

    // 获取粒子数量
    uint32_t getParticleCount() const { return _particleCount; }

    void setShaderStorageBuffers(std::vector<std::shared_ptr<VulkanBuffer>>& shaderStorageBuffers);
private:
    std::shared_ptr<VulkanContext> _vkContext;
    uint32_t _particleCount = 0;

    uint32_t _screenWidth;
    uint32_t _screenHeight;
    int      _maxFrameInFlight;


    // 【核心资源】：为支持 Frames in Flight 设计的 SSBO 数组
    // 为什么用 vector？因为有多少帧在飞，就需要多少个 SSBO，防止读写冲突
     //==============================================================
    std::vector<std::shared_ptr<VulkanBuffer>> _shaderStorageBuffers;

    // 私有初始化逻辑
    //void initParticles(std::shared_ptr<VulkanCommandManager> commandManager);
};