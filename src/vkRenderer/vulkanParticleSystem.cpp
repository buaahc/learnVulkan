#include "vulkanParticleSystem.h"
#include "vulkanContext.h"
#include "vulkanCommandManager.h"
#include "vulkanBuffer.h"
#include <random>
#include <ctime>


//缓冲区描述--对应于整块缓冲区的描述，步长，读取频率，对bindingDescription.binding（槽位号）的缓冲区进行描述
VkVertexInputBindingDescription Particle::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    //binding = 0，可以理解对几号缓冲区进行描述，后续在录制命令缓冲时(vkCmdBindVertexBuffers)，会绑定到 0 号槽位
    //vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);//第二个参数firstBinding = 0即表示 _vertexBuffer 真正“插”进了 0 号槽位，_vertexBuffer 绑定到了 0 号槽位；
    bindingDescription.binding = 0;//这套描述/规则，是专门写给第 0 号顶点缓冲区的
    bindingDescription.stride = sizeof(Particle);//步长，每次读取数据的步长
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//每个顶点步进一次，如果是实例化数组将是每个实例步进一次
    return bindingDescription;
    /*
    // 描述 0 号槽位怎么读
        bindingDescription0.binding = 0;
        bindingDescription0.stride = sizeof(glm::vec3); // 只包含位置

    // 描述 1 号槽位怎么读
        bindingDescription1.binding = 1;
        bindingDescription1.stride = sizeof(glm::vec3) + sizeof(glm::vec2); // 包含法线和UV

    // 稍后绑定的时候同时插入两个槽位：
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, {posBuffer, normalBuffer}, {0, 0});
    */
}

//顶点属性描述--描述缓冲区中每个顶点内部的具体数据字段（例如：位置、颜色、法线等），以及它们如何与着色器（GLSL）中的 layout(location = x) 对应。
std::vector<VkVertexInputAttributeDescription> Particle::getAttributeDescriptions() {

    //VkVertexInputAttributeDescription通过binding连接缓冲区，通过location连接着色器中的变量
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    {
        VkVertexInputAttributeDescription attributeDescription;
        //attributeDescription.binding表示从哪块缓冲区读取数据
        attributeDescription.binding = 0;//即上面VkVertexInputBindingDescription描述中，索引0的内存数据
        // attributeDescription.location表述数据传递给着色器中的location= x的变量
        attributeDescription.location = 0;//这个就是vertexShader--layout(location = 0)
        attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescription.offset = offsetof(Particle, _position);
        attributeDescriptions.push_back(attributeDescription);
    }


    {
        VkVertexInputAttributeDescription attributeDescription;
        //attributeDescription.binding表示从哪块缓冲区读取数据
        attributeDescription.binding = 0;//即上面VkVertexInputBindingDescription描述中，索引0的内存数据
        // attributeDescription.location表述数据传递给着色器中的location= x的变量
        attributeDescription.location = 1;//这个就是vertexShader--layout(location = 1)
        attributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescription.offset = offsetof(Particle, _color);
        attributeDescriptions.push_back(attributeDescription);
    }

    
    //{
    //    VkVertexInputAttributeDescription attributeDescription;
    //    //attributeDescription.binding表示从哪块缓冲区读取数据
    //    attributeDescription.binding = 0;//即上面VkVertexInputBindingDescription描述中，索引0的内存数据
    //    // attributeDescription.location表述数据传递给着色器中的location= x的变量
    //    attributeDescription.location = 1;//这个就是vertexShader--layout(location = 1)
    //    attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
    //    attributeDescription.offset = offsetof(Particle, _velocity);
    //    attributeDescriptions.push_back(attributeDescription);
    //}
    return attributeDescriptions;
}

VulkanParticleSystem::VulkanParticleSystem(
    std::shared_ptr<VulkanContext> context,
    std::shared_ptr<VulkanCommandManager> commandManager,
    uint32_t particleCount,
    const uint32_t screenWidth,
    const uint32_t screenHeight,
    int maxFrameInFlight
)
{
    this->_vkContext = context;
    this->_particleCount = particleCount;
    this->_screenWidth = screenWidth;
    this->_screenHeight = screenHeight;
    this->_maxFrameInFlight = maxFrameInFlight;
    //this->initParticles(commandManager);
}

void VulkanParticleSystem::bindBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
    VkBuffer vertexBuffers[] = { this->_shaderStorageBuffers[currentFrame]->getBuffer()};
    VkDeviceSize offsets[] = { 0 };
    uint32_t firstBinding = 0;
    uint32_t bindingCount = 1;
    //vertexBuffers缓冲区插进0号“槽位”，即将vertexBuffers与binding=0的缓冲区描述连接起来
    vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, vertexBuffers, offsets);
}

// 发出绘制指令 (无索引绘制)
void VulkanParticleSystem::draw(VkCommandBuffer commandBuffer)
{
    vkCmdDraw(commandBuffer, this->_particleCount, 1, 0, 0);
}

void VulkanParticleSystem::setShaderStorageBuffers(std::vector<std::shared_ptr<VulkanBuffer>>& shaderStorageBuffers)
{
    this->_shaderStorageBuffers = shaderStorageBuffers;
}