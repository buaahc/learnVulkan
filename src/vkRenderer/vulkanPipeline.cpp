#include "vulkanPipeline.h"
#include "vulkanContext.h"
#include "vulkanCommandManager.h"
#include "readBuffer.h"
#include "tools.h"

VulkanPipeline::VulkanPipeline(
    std::shared_ptr<VulkanContext> context,
    const std::string& vertFilepath,
    const std::string& fragFilepath,
    const PipelineConfigInfo& configInfo,
    VkDescriptorSetLayout descriptorSetLayout,
    VkRenderPass renderPass,
    uint32_t subpass)
{
    this->_vkContext = context;
    this->_vertFilepath = vertFilepath;
    this->_fragFilepath = fragFilepath;
    this->_pipelineConfigInfo = configInfo;
    this->_descriptorSetLayout = descriptorSetLayout;
    this->_renderPass = renderPass;
    this->_subpass = subpass;
    this->createPipeline();
}

// (可选) 专门为 Compute Shader 准备的构造函数
VulkanPipeline::VulkanPipeline(
    std::shared_ptr<VulkanContext> context,
    const std::string& compFilepath,
    VkDescriptorSetLayout descriptorSetLayout)
{
    this->_vkContext = context;
    this->_compFilepath = compFilepath;
    this->_descriptorSetLayout = descriptorSetLayout;
    this->createComputePipeline();
}

VulkanPipeline::~VulkanPipeline()
{
    vkDestroyPipeline(this->_vkContext->getLogicDevice(), this->_vkPipeline, nullptr);
    vkDestroyPipelineLayout(this->_vkContext->getLogicDevice(), this->_pipelineLayout, nullptr);
}

std::string VulkanPipeline::vertFilepath()
{
    return this->_vertFilepath;
}

std::string VulkanPipeline::fragFilepath()
{
    return this->_fragFilepath;
}

std::string VulkanPipeline::compFilepath()
{
    return this->_compFilepath;
}

void VulkanPipeline::createPipeline()
{
    VkShaderModule vertShaderModule = vkDB::createShaderModule(readFile(this->_vertFilepath), this->_vkContext->getLogicDevice());
    VkShaderModule fragShaderModule = vkDB::createShaderModule(readFile(this->_fragFilepath), this->_vkContext->getLogicDevice());

    //封装顶点着色器
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";//着色器的入口函数

    //片元着色器
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";//着色器的入口函数

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };



    //组装真正的渲染管线
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    //引用着色器功能
    pipelineInfo.pStages = shaderStages;

    //引用所有的固定功能
    pipelineInfo.pVertexInputState = &this->_pipelineConfigInfo.vertexInputInfo;//顶点缓冲区描述
    pipelineInfo.pInputAssemblyState = &this->_pipelineConfigInfo.inputAssemblyInfo;//绘制图元
    pipelineInfo.pViewportState = &this->_pipelineConfigInfo.viewportInfo;
    pipelineInfo.pRasterizationState = &this->_pipelineConfigInfo.rasterizationInfo;
    pipelineInfo.pMultisampleState = &this->_pipelineConfigInfo.multisampleInfo;
    pipelineInfo.pDepthStencilState = &this->_pipelineConfigInfo.depthStencilInfo;
    pipelineInfo.pColorBlendState = &this->_pipelineConfigInfo.colorBlendInfo;
    pipelineInfo.pDynamicState = &this->_pipelineConfigInfo.dynamicStateInfo;


    //引用管道布局
    //管道布局-全局变量使用-uniform使用
    /**
    * 管线布局用于告诉 GPU，着色器将会使用哪些全局变量（如 Uniform Buffers，通常用来传递 MVP 变换矩阵；或者 Push Constants，用于传递少量的高频更新数据）。
    * 目前是空布局（Count=0），因为最简单的三角形还不需要传递这些参数。但即使为空，Vulkan 也要求必须创建一个 VkPipelineLayout 对象。
    */
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //ubo/imageSamplers等全局变量描述符绑定到管线
    //重点：描述符图纸绑定给管线，管线就知道ubo/texture长什么样了，(这对应了 GLSL 着色器里的 layout(binding = 0) uniform UBO { ... } 和 layout(binding = 1) uniform sampler2D texSampler;)
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;

    if (this->_descriptorSetLayout)
    {
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &this->_descriptorSetLayout;
    }

    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(this->_vkContext->getLogicDevice(), &pipelineLayoutInfo, nullptr, &this->_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    pipelineInfo.layout = this->_pipelineLayout;
    //当前的vkPipeline引用渲染pass和子通道--图形管线必须知道它将在哪种渲染通道中工作（比如颜色附件的格式是什么，有没有深度缓冲等），管线和 RenderPass 是高度绑定的。
    pipelineInfo.renderPass = this->_renderPass;
    pipelineInfo.subpass = this->_subpass;

    //继承某个管线
    /**
    * 创建管线是一个非常消耗 CPU 性能的操作,Vulkan 中只要状态（哪怕只是一个深度测试开关）不一样，就必须创建一个全新的图形管线（VkPipeline）,
    * 如果两个管线A/B 99%的状态（着色器、顶点格式、混合模式）完全一模一样。
    * 如果你从头开始分别创建 A 和 B，驱动程序可能会做很多重复的苦力活。
    * 利用管线派生，你可以告诉驱动：“我要创建管线 B，它大部分状态和管线 A 一样，你直接把管线 A 复制过来，只改一下线框模式就行了。”
    * 这样可以大大加快管线 B 的创建速度，并可能节省显存。
    * 但是vkCreateGraphicsPipelines的第二个参数pipelineCache管道缓存，比管道继承更先进。
    */
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    //创建渲染管线
    //pipelineCache-管道缓存可用于存储和重用与管道创建相关的数据，以便在多次调用 vkCreateGraphicsPipelines甚至程序执行之间重复使用
    //pipelineCache-管道缓存比上面的管线继承更先进，管线缓存不仅能实现类似于派生的性能优化，还能把编译好的管线保存到硬盘上，下次玩家启动游戏时直接读取，实现真正的“秒开
    if (vkCreateGraphicsPipelines(this->_vkContext->getLogicDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->_vkPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

void VulkanPipeline::createComputePipeline()
{
    VkShaderModule computeShaderModule = vkDB::createShaderModule(readFile(this->_compFilepath), this->_vkContext->getLogicDevice());

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;

    if (this->_descriptorSetLayout)
    {
        //重点：描述符图纸绑定给管线，管线就知道ubo/texture/ssbo长什么样了，(这对应了 GLSL 着色器里的 layout(binding = 0) uniform UBO { ... } 和 layout(binding = 1) uniform sampler2D texSampler;)
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &this->_descriptorSetLayout;
    }

    if (vkCreatePipelineLayout(this->_vkContext->getLogicDevice(), &pipelineLayoutInfo, nullptr, &this->_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = this->_pipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(this->_vkContext->getLogicDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->_vkPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    vkDestroyShaderModule(this->_vkContext->getLogicDevice(), computeShaderModule, nullptr);
}

void VulkanPipeline::bindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint)
{
    vkCmdBindPipeline(commandBuffer, bindPoint, this->_vkPipeline);
}

VkPipelineLayout VulkanPipeline::getPipelineLayout()
{
    return this->_pipelineLayout;
}

void VulkanPipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo)
{
    //绘制的图元类型-即glDrawArray/glDrawElement函数中的第一个参数，绘制指令vkDrawCmd/vkCmdDrawIndexed并不指定图元类型
    configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    //视口与裁切--为什么没有定义具体的宽高？ 因为代码后面使用了动态状态（Dynamic State），具体的宽高可以在绘制命令录制时动态指定。
    configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    configInfo.viewportInfo.viewportCount = 1;
    configInfo.viewportInfo.scissorCount = 1;

    //光栅化相关
    configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
    configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    configInfo.rasterizationInfo.lineWidth = 1.0f;
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    //逆时针绘制
    configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;

    //多重采样-用于抗锯齿
    configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;//样本着色
    configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;


    //启用深度测试
    configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    //指定自定义的深度范围，简而言之，它允许您仅保留落在指定深度范围内的片段
    configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    configInfo.depthStencilInfo.minDepthBounds = 0.0f; // Optional
    configInfo.depthStencilInfo.maxDepthBounds = 1.0f; // Optional
    //模板缓冲区相关
    configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
    configInfo.depthStencilInfo.front = {}; // Optional
    configInfo.depthStencilInfo.back = {}; // Optional


    //颜色混合-针对渲染子通道中的帧缓冲附件，如果子通道有多个（N个）附件（MRT），那此处就需要创建多个（N个）对应的VkPipelineColorBlendAttachmentState，以便对每个输出附件进行混合设置
    //VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    //glColorMask
    configInfo.colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    configInfo.colorBlendAttachment.blendEnable = VK_FALSE;

    //VkPipelineColorBlendStateCreateInfo colorBlending{};
    configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
    configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    configInfo.colorBlendInfo.attachmentCount = 1;
    configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
    configInfo.colorBlendInfo.blendConstants[0] = 0.0f;
    configInfo.colorBlendInfo.blendConstants[1] = 0.0f;
    configInfo.colorBlendInfo.blendConstants[2] = 0.0f;
    configInfo.colorBlendInfo.blendConstants[3] = 0.0f;

    //视口与裁切测试-允许动态状态修改，渲染过程中可修改
    /**
    * 前面说过，Vulkan 几乎所有的状态都要提前烘焙，如果你调整了窗口大小，视口（Viewport）改变了，难道要销毁并重新创建一个庞大的管线吗？
    * 为了解决这个问题，Vulkan 允许少数状态被标记为“动态的”，这里将视口和裁剪框设为动态，
    * 意味着可以在渲染时（Command Buffer中）调用 vkCmdSetViewport 随时改变它们的大小，而无需重建管线。
    */
    configInfo.dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
    configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
}

void VulkanPipeline::particlePipelineConfigInfo(PipelineConfigInfo& configInfo)
{
    VulkanPipeline::defaultPipelineConfigInfo(configInfo);
    configInfo.setInputAssemblyTopology(VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    configInfo.setBlendState(true);
    configInfo.setDepthTestState(false, false);
}


void PipelineConfigInfo::setInputAssemblyTopology(VkPrimitiveTopology vkPrimitiveTopology)
{
    this->inputAssemblyInfo.topology = vkPrimitiveTopology;
}

void PipelineConfigInfo::setMsaaSampler(VkSampleCountFlagBits msaaSampler)
{
    this->multisampleInfo.rasterizationSamples = msaaSampler;
}

void PipelineConfigInfo::setBlendState(bool blend)
{
    if (blend)
    {
        this->colorBlendAttachment.blendEnable = VK_TRUE;
        this->colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        this->colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        this->colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        this->colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        this->colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        this->colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    }
    else
    {
        this->colorBlendAttachment.blendEnable = VK_FALSE;
    }
}

void PipelineConfigInfo::setDepthTestState(bool testDepth, bool wirteDepth)
{
    VkBool32 depthTestEnable = VK_TRUE;
    if (!testDepth)
    {
        depthTestEnable = VK_FALSE;
    }
    VkBool32 depthWriteEnable = VK_TRUE;
    if (!wirteDepth)
    {
        depthWriteEnable = VK_FALSE;
    }
    this->depthStencilInfo.depthTestEnable = depthTestEnable;
    this->depthStencilInfo.depthWriteEnable = depthWriteEnable;
}

void PipelineConfigInfo::setVertexBindingDescription(std::vector<VkVertexInputBindingDescription> bindingDescriptions,
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions)
{
    this->_bindingDescriptions = bindingDescriptions;
    this->_attributeDescriptions = attributeDescriptions;

    //把顶点数据内存块（VkBuffer）的描述绑定到管线上，一个 Binding 描述了整个数据块的总体特征：比如相邻两个顶点之间的字节间距（Stride 是多少），以及数据是逐个顶点提取，还是逐个实例（Instance）提取
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = this->_bindingDescriptions.size();//绑定的数据
    vertexInputInfo.pVertexBindingDescriptions = this->_bindingDescriptions.data(); // Optional

    //属性描述绑定到管线上
    // Attribute（属性）：一个顶点通常包含多种信息，比如位置（x,y,z）、颜色（r,g,b）、纹理坐标（u,v）等。
    // 这些具体的细节就是属性。属性描述用来告诉 GPU：“位置信息在这个数据块的第 0 个字节，类型是 vec3；颜色信息在第 12 个字节，类型也是 vec3”。
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());//数据的属性
    vertexInputInfo.pVertexAttributeDescriptions = this->_attributeDescriptions.data(); // Optional
}
