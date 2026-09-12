// vulkanPipeline.h (架构草案，请参考)
#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

class VulkanContext;

// 这是一个纯数据结构，保存了创建管线所需的所有蓝图信息
struct PipelineConfigInfo {
    // 禁止拷贝，因为有些结构体内部包含指针，浅拷贝会出问题
    PipelineConfigInfo(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;
    // default 告诉编译器生成默认的，无参数的构造函数
    PipelineConfigInfo() = default;

    //绑定顶点缓冲区描述
    std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
    //绑定顶点缓冲区属性描述
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    VkPipelineViewportStateCreateInfo viewportInfo{};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    std::vector<VkDynamicState> dynamicStateEnables{};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};

    // 管线布局：决定了 Shader 怎么接收外部数据 (UBO/Texture)
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    // 渲染通道：管线必须知道它在哪张画布上工作
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
};

class VulkanPipeline {
public:
    // 构造函数：负责读取 shader 文件，并根据 configInfo 创建真正的硬件管线
    VulkanPipeline(
        std::shared_ptr<VulkanContext> context,
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        const PipelineConfigInfo& configInfo
    );

    // (可选) 专门为 Compute Shader 准备的构造函数
    VulkanPipeline(
        std::shared_ptr<VulkanContext> context,
        const std::string& compFilepath,
        VkPipelineLayout pipelineLayout
    );

    ~VulkanPipeline();

    // 禁用拷贝语义
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    // ----- 核心操作 -----

    // 将此管线绑定到当前的 CommandBuffer 上 (替换 vkCmdBindPipeline)
    void bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

    // 获取底层句柄 (一般不需要，因为提供了 bind 方法)
    VkPipeline getPipeline() const { return _graphicsPipeline; }

    // ----- 极其方便的配置生成器 (工厂方法) -----

    // 生成一套标准的、用来画 3D 模型的默认配置（开启深度测试，三角形拓扑）
    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);

    // 生成一套用来画粒子系统的特殊配置（关闭深度测试，点拓扑）
    static void particlePipelineConfigInfo(PipelineConfigInfo& configInfo);

private:
    std::shared_ptr<VulkanContext> _vkContext;
    VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

    // 临时持有的 Shader Module，管线创建完即可销毁
    VkShaderModule _vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule _fragShaderModule = VK_NULL_HANDLE;

    // 私有辅助方法
    std::vector<char> readFile(const std::string& filepath);
    void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
};