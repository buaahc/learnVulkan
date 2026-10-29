#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "vulkanContext.h"

// 引入我们封装的高级组件
class VulkanContext;
class VulkanCommandManager;
class VulkanSwapChain;
class VulkanDescriptorManager;
class VulkanDescriptorBuilder;
class VulkanPipeline;
class VulkanTexture;
class VulkanModel;
class VulkanBuffer;
class VulkanParticleSystem;

//ubo
struct UniformBufferObject {
    float deltaTime = 1.0f;
};

//ubo
struct UniformBufferObject_LoadModel {
    glm::mat4 _model;
    glm::mat4 _view;
    glm::mat4 _proj;
};

class VulkanApp {
public:
    void run();
private:
    void initVulkan();
    void mainLoop();
    void cleanup();
    void drawFrame();
    
    // 更新统一的数据 (如 MVP 矩阵)
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage);

    void createSSBOBuffers(int particleCount);


    void createModelDescriptorSets();
    void createComputeDescriptorSets();

    void createTexture();
    void createModel();
    void createParticleSystem(uint32_t particleCount);

    void createModelPipeline();
    void createParticleGraphicsPipeline();
    void createComputePipeline();

    void createCommandBuffer();

    void recordCommandBuffer(
        VkCommandBuffer commandBuffer, 
        uint32_t imageIndex);

    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer);

    void createSyncObjects();

private:
    // =====================================
    // 1. 核心系统底座 (三大天王)
    // =====================================
    std::shared_ptr<VulkanContext> _vkContext;
    std::shared_ptr<VulkanCommandManager> _vkCommandManager;
    std::shared_ptr<VulkanSwapChain> _vkSwapChain;

    // =====================================
    // 2. 描述符系统 (资源管家)
    // =====================================
    std::shared_ptr<VulkanDescriptorManager> _vkDescriptorManager;
    std::shared_ptr<VulkanDescriptorBuilder> _vkDescriptorBuilder;

    // 存放模型和粒子分配出来的 DescriptorSets
    //==================================================
    std::vector<VkDescriptorSet> _modelDescriptorSets;
    VkDescriptorSetLayout _modelDescriptorLayout = VK_NULL_HANDLE;
    
    std::vector<VkDescriptorSet> _computeDescriptorSets;
    VkDescriptorSetLayout _computeDescriptorLayout = VK_NULL_HANDLE;

    // =====================================
    // 3. 业务资产 (Assets)
    // =====================================
    std::shared_ptr<VulkanTexture> _woodTexture;
    std::shared_ptr<VulkanModel> _vikingRoomModel;
    std::shared_ptr<VulkanParticleSystem> _particleSystem;
    
    // UBO
    std::vector<std::shared_ptr<VulkanBuffer>> _modelUboBuffers;
    std::vector<std::shared_ptr<VulkanBuffer>> _particleUboBuffers;

    //ssbo
    std::vector<std::shared_ptr<VulkanBuffer>> _shaderStorageBuffers;

    // =====================================
    // 4. 管线系统 (Pipelines)
    // =====================================
    std::shared_ptr<VulkanPipeline> _modelPipeline;
    std::shared_ptr<VulkanPipeline> _particleGraphicsPipeline; // 用于画粒子的 Graphics 管线
    std::shared_ptr<VulkanPipeline> _computePipeline;  // 用于算粒子的 Compute 管线

    // =====================================
    // 5. 渲染循环必须品
    // =====================================
    std::vector<VkCommandBuffer> _drawCommandBuffers;
    std::vector<VkCommandBuffer> _computeCommandBuffers;
    
    // 同步对象 (原本在 compute.h 里的信号量和栅栏，保留在主程序中控制循环节拍最为合适)
    std::vector<VkSemaphore> _imageAvailableSemaphores;//呈现引擎读取完毕--交换链可被写入
    
    std::vector<VkSemaphore> _renderFinishedSemaphores;//绘制引擎渲染完毕--呈现引擎可以展示
    std::vector<VkFence> _inFlightFences;
    
    std::vector<VkSemaphore> _computeFinishedSemaphores;//计算着色器渲染完毕--绘制引擎可以使用
    std::vector<VkFence> _computeInFlightFences;

    
    uint32_t _currentFrame = 0;

    float _lastFrameTime = 0.0f;
    double _lastTime = 0.0f;

};