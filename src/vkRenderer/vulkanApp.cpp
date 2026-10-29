#include <chrono>
#include <random>
#include <ctime>
#include "vulkanCommandManager.h"
#include "vulkanDescriptor.h"
#include "vulkanBuffer.h"
#include "vulkanSwapChain.h"
#include "vulkanTexture.h"
#include "vulkanParticleSystem.h"
#include "vulkanPipeline.h"
#include "vulkanModel.h"
#include "tools.h"
#include "vulkanApp.h"


const int MAX_FRAMES_IN_FLIGHT = 2;
const uint32_t PARTICLE_COUNT = 8192;
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "images/viking_room.png";

void VulkanApp::initVulkan()
{
    /**this->_vkContext:
    * window/instance/validLayer/_vkSurface
    * physicalDevice/logicDevice
    * graphicQueue/computeQueue/presentQueue
    */
    this->_vkContext = std::make_shared<VulkanContext>(800, 600, "vulkanApp");
    this->_lastTime = this->_vkContext->getFrameTime();

    //commandPool/commandBuffer
    this->_vkCommandManager = std::make_shared<VulkanCommandManager>(this->_vkContext);

    this->_vkSwapChain = std::make_shared<VulkanSwapChain>(this->_vkContext, this->_vkCommandManager);

    this->_vkDescriptorManager = std::make_shared<VulkanDescriptorManager>(this->_vkContext);
    this->_vkDescriptorBuilder = std::make_shared<VulkanDescriptorBuilder>(this->_vkContext, this->_vkDescriptorManager);


    this->createTexture();
    this->createModel();
    this->createParticleSystem(PARTICLE_COUNT);

    this->createUniformBuffers();

    this->createModelDescriptorSets();
    this->createComputeDescriptorSets();
    this->createModelPipeline();
    this->createParticleGraphicsPipeline();
    this->createComputePipeline();
    this->createCommandBuffer();
    this->createSyncObjects();
}

void VulkanApp::mainLoop()
{
    while (!glfwWindowShouldClose(this->_vkContext->getWindow())) {
        //处理事件输入
        glfwPollEvents();
        this->drawFrame();

        double currentTime = this->_vkContext->getFrameTime();
        this->_lastFrameTime = (currentTime - this->_lastTime) * 1000.0;
        this->_lastTime = currentTime;
    }
    //cpu阻塞，确保GPU全部指令执行完毕，处于闲置（idle）状态，cpu才继续运行，执行程序退出
    vkDeviceWaitIdle(this->_vkContext->getLogicDevice());

}

void VulkanApp::cleanup()
{
}

void VulkanApp::run() {
    //initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

//创建uniform缓冲区
void VulkanApp::createUniformBuffers()
{
    //VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT：就是任务管理器GPU界面中的共享GPU内存（就是真实的内存条）, CPU可以通过vkMapMemory直接访问，但是GPU读取会比较慢，因为每一帧都需要更新所以使用暂存缓冲区失去意义
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
        {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject_LoadModel);
            std::shared_ptr<VulkanBuffer>uboBuffer = std::make_shared<VulkanBuffer>(
                this->_vkContext, 
                this->_vkCommandManager, 
                bufferSize, 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            this->_modelUboBuffers.push_back(uboBuffer);
        }
        {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject);
            std::shared_ptr<VulkanBuffer>uboBuffer = std::make_shared<VulkanBuffer>(
                this->_vkContext, 
                this->_vkCommandManager,
                bufferSize, 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            this->_particleUboBuffers.push_back(uboBuffer);
        }
    }
}

void VulkanApp::updateUniformBuffer(uint32_t currentImage)
{
    if (this->_modelUboBuffers.size() > currentImage && this->_particleUboBuffers.size() > currentImage)
    {
        {
            std::shared_ptr<VulkanBuffer> uboBuffer = this->_particleUboBuffers.at(currentImage);
            if (uboBuffer)
            {
                UniformBufferObject ubo{};
                ubo.deltaTime = this->_lastFrameTime * 2.0f;
                uboBuffer->setBufferData(&ubo, sizeof(ubo));
            }
        }

        {
            std::shared_ptr<VulkanBuffer> uboBuffer = this->_modelUboBuffers.at(currentImage);
            if (uboBuffer)
            {
                static auto startTime = std::chrono::high_resolution_clock::now();

                auto currentTime = std::chrono::high_resolution_clock::now();
                float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

                UniformBufferObject_LoadModel ubo{};
                ubo._model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

                ubo._view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                ubo._proj = glm::perspective(glm::radians(45.0f), this->_vkSwapChain->getSwapChainExtent().width / (float)this->_vkSwapChain->getSwapChainExtent().height, 0.1f, 10.0f);
                ubo._proj[1][1] *= -1;
                uboBuffer->setBufferData(&ubo, sizeof(ubo));
            }
        }
    }
}

void VulkanApp::createSSBOBuffers(int particleCount)
{
    // Initialize particles
    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    // Initial particle positions on a circle
    std::vector<Particle> particles(particleCount);
    for (auto& particle : particles) {
        float r = 0.25f * sqrt(rndDist(rndEngine));
        float theta = rndDist(rndEngine) * 2 * 3.14159265358979323846;
        float x = r * cos(theta) * (float)this->_vkSwapChain->getSwapChainExtent().height / (float)this->_vkSwapChain->getSwapChainExtent().height;
        float y = r * sin(theta);
        particle._position = glm::vec2(x, y);
        particle._velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
        particle._color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    }

    VkDeviceSize bufferSize = sizeof(Particle) * particleCount;
    //数据暂存缓冲区
    VulkanBuffer vulkanBuffer(
        this->_vkContext,
        this->_vkCommandManager,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vulkanBuffer.setBufferData(particles.data(), bufferSize, 0);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        std::shared_ptr<VulkanBuffer> vulkanBufer = std::make_shared<VulkanBuffer>(
            this->_vkContext,
            this->_vkCommandManager,
            bufferSize,
            /**
            * VK_BUFFER_USAGE_STORAGE_BUFFER_BIT (存储缓冲区标志)
            * 用途：允许该缓冲区在着色器（Shader）中作为 存储缓冲区（Storage Buffer / SSBO） 被绑定和使用。
            * 特点：
            * 可读可写：与只能读取的统一缓冲区（Uniform Buffer, UBO）不同，着色器（特别是计算着色器 Compute Shader）可以随时读取和写入这种缓冲区。
            * 容量大：它通常用于传递或存储大量数据，不受 UBO 严格的大小限制。
            * 常见场景：在着色器中保存物理模拟的数据、存储游戏世界中所有对象的变换矩阵、或者作为计算着色器输出计算结果的地方。
            */
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        // Copy data from the staging buffer (host) to the shader storage buffer (GPU)
        vulkanBuffer.copyToBuffer(vulkanBufer->getBuffer());
        this->_shaderStorageBuffers.push_back(vulkanBufer);
    }
}

void VulkanApp::createModelDescriptorSets()
{
#if 0
    //说明书
    //VkDescriptorSetLayoutBinding 是对 Shader 中 “一个 layout(binding = X) 槽位” 的描述；
    //layout(binding = 0) uniform sampler2D myTexture[10];VkDescriptorSetLayoutBinding就是对myTexture[10]的描述，此时VkDescriptorSetLayoutBinding需要包含十块数据
    //创建uniform buffer object描述符
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    //1-binding = 0：槽位编号（插孔编号），这严格对应了在 GLSL 着色器代码里写的 layout(binding = 0)
    uboLayoutBinding.binding = 0;
    //2-descriptorType：描述符类型（UBO）
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //descriptorCount表示 layout(binding = X) 这个槽位需要创建多少个描述符，一个描述符就是一个指针，它只能指向 1 块具体的显存资源（1 个 Buffer 或者 1 张 Texture）。
    //layout(set = 0, binding = 1) uniform sampler2D textures[10];就需要descriptorCount= 10，表示创建10个描述符，指向十块指针内存
     // 这里只是单一的 UBO，所以是 descriptorCount = 1，如果你在着色器里声明了一个 UBO 数组，这里就填数组长度。。
    uboLayoutBinding.descriptorCount = 1;
    //4-指定ubo对象用于哪个着色器阶段
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    //用于image sampling
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    //创建纹理图像描述符
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    //image-texture
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
#endif // 0
    if (!this->_vkDescriptorBuilder)
        return;
    //this->_vkDescriptorBuilder->reset();
    this->_modelDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {

        //ubo描述符对应的buffer，告诉它真正的资源在哪（具体的 ubo 内存地址）
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = this->_modelUboBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = this->_modelUboBuffers[i]->getBufferSize();
        this->_vkDescriptorBuilder->bindBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, &bufferInfo);

        if (this->_woodTexture)
        {
            //纹理描述符对应的buffer
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = this->_woodTexture->getImageView();
            imageInfo.sampler = this->_woodTexture->getSampler();
            this->_vkDescriptorBuilder->bindImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &imageInfo);
        }
        this->_vkDescriptorBuilder->build(this->_modelDescriptorSets[i],this->_modelDescriptorLayout);
    }
}

void VulkanApp::createComputeDescriptorSets()
{
    if (!this->_vkDescriptorBuilder)
        return;
    this->_computeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        {
            //ubo描述符对应的buffer，告诉它真正的资源在哪（具体的 ubo 内存地址）
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = this->_particleUboBuffers[i]->getBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = this->_particleUboBuffers[i]->getBufferSize();
            this->_vkDescriptorBuilder->bindBuffer(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, &bufferInfo);
        }
        {
            //ssbo0
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = this->_shaderStorageBuffers[(i + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT]->getBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = this->_shaderStorageBuffers[(i + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT]->getBufferSize();
            this->_vkDescriptorBuilder->bindBuffer(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, &bufferInfo);
        }
        {
            //ssbo1
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = this->_shaderStorageBuffers[i]->getBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = this->_shaderStorageBuffers[i]->getBufferSize();
            this->_vkDescriptorBuilder->bindBuffer(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, &bufferInfo);
        }
        this->_vkDescriptorBuilder->build(this->_computeDescriptorSets[i], this->_computeDescriptorLayout);
    }
}

void VulkanApp::createTexture()
{
    std::string exeDir = getExeDirectory();
    this->_woodTexture = std::make_shared<VulkanTexture>(
        this->_vkContext, 
        this->_vkCommandManager,
        exeDir + "/resources/" + TEXTURE_PATH);
}

void VulkanApp::createModel()
{
    std::string exeDir = getExeDirectory();
    this->_vikingRoomModel = std::make_shared<VulkanModel>(
        this->_vkContext,
        this->_vkCommandManager,
        exeDir + "/resources/" + MODEL_PATH);
}

void VulkanApp::createParticleSystem(uint32_t particleCount)
{
    this->_particleSystem = std::make_shared<VulkanParticleSystem>(
        this->_vkContext,
        this->_vkCommandManager,
        particleCount,
        this->_vkSwapChain->getWidth(),
        this->_vkSwapChain->getHeight(),
        MAX_FRAMES_IN_FLIGHT);
    this->createSSBOBuffers(particleCount);
    this->_particleSystem->setShaderStorageBuffers(this->_shaderStorageBuffers);
}

//模型渲染管线
void VulkanApp::createModelPipeline()
{
    std::string exeDir = getExeDirectory();
    std::string vertFilepath = exeDir + "/resources/shaders/vert.spv";
    std::string fragFilepath = exeDir + "/resources/shaders/frag.spv";

    PipelineConfigInfo pipelineConfigInfo;
    VulkanPipeline::defaultPipelineConfigInfo(pipelineConfigInfo, this->_vkContext->getMsaaSamples());
    pipelineConfigInfo.setVertexBindingDescription({ Vertex::getBindingDescription() }, { Vertex::getAttributeDescriptions() });

    this->_modelPipeline = std::make_shared<VulkanPipeline>(
        this->_vkContext,
        vertFilepath,
        fragFilepath,
        pipelineConfigInfo,
        this->_modelDescriptorLayout,
        this->_vkSwapChain->getRenderPass(),
        0);
}

//粒子渲染管线
void VulkanApp::createParticleGraphicsPipeline()
{
    std::string exeDir = getExeDirectory();
    std::string vertFilepath = exeDir + "/resources/shaders/computevert.spv";
    std::string fragFilepath = exeDir + "/resources/shaders/computefrag.spv";

    PipelineConfigInfo pipelineConfigInfo;
    VulkanPipeline::particlePipelineConfigInfo(pipelineConfigInfo, this->_vkContext->getMsaaSamples());
    pipelineConfigInfo.setVertexBindingDescription({ Particle::getBindingDescription() }, { Particle::getAttributeDescriptions() });

    this->_particleGraphicsPipeline = std::make_shared<VulkanPipeline>(
        this->_vkContext,
        vertFilepath,
        fragFilepath,
        pipelineConfigInfo,
        nullptr,
        this->_vkSwapChain->getRenderPass(),
        0);
}

//粒子计算管线
void VulkanApp::createComputePipeline()
{
    std::string exeDir = getExeDirectory();
    std::string compFilepath = exeDir + "/resources/shaders/Comp.spv";
    this->_computePipeline = std::make_shared<VulkanPipeline>(
        this->_vkContext,
        compFilepath,
        this->_computeDescriptorLayout);
}

void VulkanApp::createCommandBuffer()
{
    if (this->_vkCommandManager)
    {
        this->_drawCommandBuffers = this->_vkCommandManager->createCommandBuffers(MAX_FRAMES_IN_FLIGHT);
        this->_computeCommandBuffers = this->_vkCommandManager->createCommandBuffers(MAX_FRAMES_IN_FLIGHT);
    }
}


//关键步骤十三：录制命令缓冲区--分配好命令缓冲区之后，就要开始将要执行的命令写入到命令​​缓冲区
//在 Vulkan 中，不能直接下令“画一个三角形”。必须把所有的指令（开启渲染通道、绑定管线、设置视口、执行绘制等）按顺序“录制”到一个缓冲区里，最后一次性提交给 GPU 执行。
void VulkanApp::recordCommandBuffer(
    VkCommandBuffer commandBuffer, 
    uint32_t imageIndex)
{
    //1-开始录制命令缓冲区
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    //2-配置渲染通道renderPass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //渲染通道本身
    renderPassInfo.renderPass = this->_vkSwapChain->getRenderPass();
    //绑定的帧缓冲区
    renderPassInfo.framebuffer = this->_vkSwapChain->getFramebuffer(imageIndex);

    //渲染区域大小
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = this->_vkSwapChain->getSwapChainExtent();

    //清屏颜色/深度，必须定义的framebufer包含附件数量一一对应
    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };
    clearValues[2].color = { {0.0f, 0.0f, 0.0f, 1.0f} };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();


    /**
    *VK_SUBPASS_CONTENTS_INLINE渲染通道命令将嵌入到主命令缓冲区本身中，不会执行任何辅助命令缓冲区。
    *VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS渲染通道命令将从辅助命令缓冲区执行。
    */
    //开始渲染，这告诉 GPU 渲染正式开始
    //当你调用 vkCmdBeginRenderPass 时，你就进入了第 0 个房间（Subpass 0），所有的绘制命令都在这间房里对着指定的画布工作；当你调用 vkCmdNextSubpass 时，你就走进了下一间房。
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


    //如果你在创建管线时设置了视口（Viewport）和裁剪（Scissor）为 Dynamic State，那么你必须在录制时手动设置它们。
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(this->_vkSwapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(this->_vkSwapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = this->_vkSwapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


    // 首先绘制模型
    // bind vkPipeline
    // bind Vertex Buffer (比如 _vertexBuffer)
    // bind Index Buffer (比如 _indexBuffer)
    // bind Descriptor Set (比如 _descriptorSets[imageIndex])
    // draw
    {
        this->_modelPipeline->bindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
        this->_vikingRoomModel->bindBuffer(commandBuffer);
        
        vkCmdBindDescriptorSets(
            commandBuffer, 
            VK_PIPELINE_BIND_POINT_GRAPHICS, 
            this->_modelPipeline->getPipelineLayout(), 0, 1,
            &this->_modelDescriptorSets[this->_currentFrame],//绑定的描述符集，ubo和textureSampler数据
            0, nullptr);

        this->_vikingRoomModel->drawIndex(commandBuffer);
    }

    //绘制粒子系统
    {
        this->_particleGraphicsPipeline->bindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
        this->_particleSystem->bindBuffer(commandBuffer, this->_currentFrame);
        this->_particleSystem->draw(commandBuffer);
    }
    //渲染过程结束
    vkCmdEndRenderPass(commandBuffer);
    //命令缓冲区录制完毕--将缓冲区从“录制状态”转变为“可提交状态（Executable state）”。
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void VulkanApp::recordComputeCommandBuffer(VkCommandBuffer commandBuffer)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording compute command buffer!");
    }
    
    //bind pipeline
    //bind DescriptorSets
    //draw
    //计算着色器不绘制图形，无需bind buffer
    //=========================================================================================
    this->_computePipeline->bindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);

    vkCmdBindDescriptorSets(
        commandBuffer, 
        VK_PIPELINE_BIND_POINT_COMPUTE, 
        this->_computePipeline->getPipelineLayout(),
        0, 1,
        &this->_computeDescriptorSets[this->_currentFrame],
        0, nullptr);

    //draw
    vkCmdDispatch(commandBuffer, PARTICLE_COUNT / 256, 1, 1);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record compute command buffer!");
    }
}

//关键步骤十四：创建同步对象-信号量/栅栏，信号量用于GPU同步，栅栏用于CPU同步
void VulkanApp::createSyncObjects() {

    this->_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    this->_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    this->_computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    this->_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    this->_computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    //信号量
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    //栅栏
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    //创建栅栏时带有信号，这样就可以防止绘制函数drawFrame()第一次无限等待
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        //vkCreateSemaphore和vkCreateFence一次只能创建一个对象
        if (vkCreateSemaphore(this->_vkContext->getLogicDevice(), &semaphoreInfo, nullptr, &this->_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(this->_vkContext->getLogicDevice(), &semaphoreInfo, nullptr, &this->_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(this->_vkContext->getLogicDevice(), &fenceInfo, nullptr, &this->_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
        if (vkCreateSemaphore(this->_vkContext->getLogicDevice(), &semaphoreInfo, nullptr, &this->_computeFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(this->_vkContext->getLogicDevice(), &fenceInfo, nullptr, &this->_computeInFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute synchronization objects for a frame!");
        }
    }
}

//真正的渲染帧-渲染循环
/**从总体上看，Vulkan 渲染帧包含一系列常见的步骤：
* 等待上一帧结束
* 从交换链中获取图像
* 记录一个命令缓冲区，该缓冲区会将场景绘制到该图像上。
* 提交已记录的命令缓冲区到GPU进行绘制
* 展示交换链图像
*/

void VulkanApp::drawFrame()
{
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    //先进行计算着色器的提交-计算着色器首先运行
    // Compute submission        
    vkWaitForFences(this->_vkContext->getLogicDevice(), 1, &this->_computeInFlightFences[this->_currentFrame], VK_TRUE, UINT64_MAX);
    updateUniformBuffer(this->_currentFrame);
    vkResetFences(this->_vkContext->getLogicDevice(), 1, &this->_computeInFlightFences[this->_currentFrame]);

    vkResetCommandBuffer(this->_computeCommandBuffers[this->_currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    //录制命令缓冲区-计算着色器
    recordComputeCommandBuffer(this->_computeCommandBuffers[this->_currentFrame]);

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &this->_computeCommandBuffers[this->_currentFrame];
    //绘制完毕发出的信号量_computeFinishedSemaphores
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &this->_computeFinishedSemaphores[this->_currentFrame];
    //绘制完毕后触发栅栏信号量_computeInFlightFences

    //计算着色器命令缓冲区执行绘制完毕会同时发出_computeFinishedSemaphores/_computeInFlightFences两个信号量
    
    if (vkQueueSubmit(this->_vkContext->getComputeQueue(), 1, &submitInfo, this->_computeInFlightFences[this->_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit compute command buffer!");
    };

    //提交图形命令缓冲区指令
    // Graphics submission
    vkWaitForFences(this->_vkContext->getLogicDevice(), 1, &this->_inFlightFences[this->_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(this->_vkContext->getLogicDevice(), this->_vkSwapChain->getSwapChain(), UINT64_MAX, this->_imageAvailableSemaphores[this->_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        this->_vkSwapChain->recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(this->_vkContext->getLogicDevice(), 1, &this->_inFlightFences[this->_currentFrame]);

    vkResetCommandBuffer(this->_drawCommandBuffers[this->_currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(this->_drawCommandBuffers[this->_currentFrame], imageIndex);

    //等待_computeFinishedSemaphores-计算着色器运行完毕，_imageAvailableSemaphores-呈现引擎读取完毕（交换链可用）
    VkSemaphore waitSemaphores[] = { this->_computeFinishedSemaphores[this->_currentFrame], this->_imageAvailableSemaphores[this->_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 2;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &this->_drawCommandBuffers[this->_currentFrame];
    //绘制完毕后发出的信号量_renderFinishedSemaphores
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &this->_renderFinishedSemaphores[this->_currentFrame];
    //绘制完毕后发出的信号量_renderFinishedSemaphores和_inFlightFences两个信号量
    if (vkQueueSubmit(this->_vkContext->getGraphicsQueue(), 1, &submitInfo, this->_inFlightFences[this->_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    //提交给呈现引擎
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &this->_renderFinishedSemaphores[this->_currentFrame];

    VkSwapchainKHR swapChains[] = { this->_vkSwapChain->getSwapChain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(this->_vkContext->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || this->_vkContext->_framebufferResized) {
        this->_vkContext->_framebufferResized = false;
        this->_vkSwapChain->recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    this->_currentFrame = (this->_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
