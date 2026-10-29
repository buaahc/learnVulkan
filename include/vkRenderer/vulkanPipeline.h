// vulkanPipeline.h (架构草案，请参考)
#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

class VulkanContext;

// 这是一个纯数据结构，保存了创建管线所需的所有蓝图信息
struct PipelineConfigInfo {
    // default 告诉编译器生成默认的，无参数的构造函数
    PipelineConfigInfo() = default;

    //绑定顶点缓冲区描述
    std::vector<VkVertexInputBindingDescription> _bindingDescriptions{};
    //绑定顶点缓冲区属性描述
    std::vector<VkVertexInputAttributeDescription> _attributeDescriptions{};

    //缓冲区描述--描述了将传递给顶点着色器的缓冲区
    //把包含顶点数据的缓冲区（VkBuffer）整体描述绑定到管线上，
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

    //绘制的图元类型-即glDrawArray/glDrawElement函数中的第一个参数，绘制指令vkDrawCmd/vkCmdDrawIndexed并不指定图元类型
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};

    //视口与裁切相关
    VkPipelineViewportStateCreateInfo viewportInfo{};

    //光栅化相关
    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};

    //多重采样-用于抗锯齿
    VkPipelineMultisampleStateCreateInfo multisampleInfo{};

    //颜色混合-针对渲染子通道中的帧缓冲附件，设置其是否透明
    /**
    * glEnable(GL_BLEND);
    * glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    * glBlendEquation(GL_FUNC_ADD);
    */
    //如果子通道有多个附件（MRT），那此处就需要创建多个对应的VkPipelineColorBlendAttachmentState，以便对每个输出附件进行混合设置
    //td::array<VkPipelineColorBlendAttachmentState, 3> blendAttachments{};//顺序与MRT顺序要完全一致
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};

    //深度与模板缓冲区相关
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};


    //视口与裁切测试-允许动态状态修改，渲染过程中可修改
    /**
    * 前面说过，Vulkan 几乎所有的状态都要提前烘焙，如果你调整了窗口大小，视口（Viewport）改变了，难道要销毁并重新创建一个庞大的管线吗？
    * 为了解决这个问题，Vulkan 允许少数状态被标记为“动态的”，这里将视口和裁剪框设为动态，
    * 意味着可以在渲染时（Command Buffer中）调用 vkCmdSetViewport 随时改变它们的大小，而无需重建管线。
    */
    std::vector<VkDynamicState> dynamicStateEnables{};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};

#if 0
    // 管线布局：决定了 Shader 怎么接收外部数据 (UBO/Texture)
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    // 渲染通道：管线必须知道它在哪张画布上工作
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
#endif // 0

    //设置图元类型
    void setInputAssemblyTopology(VkPrimitiveTopology vkPrimitiveTopology);
    void setMsaaSampler(VkSampleCountFlagBits msaaSampler);
    void setBlendState(bool blend);
    void setDepthTestState(bool testDepth, bool wirteDepth);

    void setVertexBindingDescription(std::vector<VkVertexInputBindingDescription> bindingDescriptions,
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions);
};

class VulkanPipeline {
public:
    // 构造函数：负责读取 shader 文件，并根据 configInfo 创建真正的硬件管线
    VulkanPipeline(
        std::shared_ptr<VulkanContext> context,
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        const PipelineConfigInfo& configInfo,
        VkDescriptorSetLayout descriptorSetLayout,
        VkRenderPass renderPass,
        uint32_t subpass);

    // (可选) 专门为 Compute Shader 准备的构造函数
    VulkanPipeline(
        std::shared_ptr<VulkanContext> context,
        const std::string& compFilepath,
        VkDescriptorSetLayout descriptorSetLayout);

    ~VulkanPipeline();

    // 禁用拷贝语义
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    // ----- 核心操作 -----
    // 将此管线绑定到当前的 CommandBuffer 上 (替换 vkCmdBindPipeline)
    void bindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint);

    // 获取底层句柄 (一般不需要，因为提供了 bind 方法)
    VkPipeline getPipeline() const { return _vkPipeline; }

    // ----- 极其方便的配置生成器 (工厂方法) -----
    // 生成一套标准的、用来画 3D 模型的默认配置（开启深度测试，三角形拓扑
    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo, VkSampleCountFlagBits msaaSampler);

    // 生成一套用来画粒子系统的特殊配置（关闭深度测试，点拓扑）
    static void particlePipelineConfigInfo(PipelineConfigInfo& configInfo, VkSampleCountFlagBits msaaSampler);

    std::string vertFilepath();
    std::string fragFilepath();
    std::string compFilepath();

    VkPipelineLayout getPipelineLayout();
private:
    VkPipeline _vkPipeline = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

    std::shared_ptr<VulkanContext> _vkContext;

    std::string _vertFilepath = "";
    std::string _fragFilepath = "";
    std::string _compFilepath = "";

    PipelineConfigInfo _pipelineConfigInfo;

    VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
    VkRenderPass _renderPass = VK_NULL_HANDLE;
    uint32_t _subpass = 0;


    // 私有辅助方法
    void createPipeline();
    void createComputePipeline();
};