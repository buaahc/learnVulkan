#include "vulkanModel.h"
#include "vulkanBuffer.h"
#include "vulkanContext.h"
#include "vulkanCommandManager.h"
#include "readBuffer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

//缓冲区描述--对应于整块缓冲区的描述，步长，读取频率，对bindingDescription.binding（槽位号）的缓冲区进行描述
VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    //binding = 0，可以理解对几号缓冲区进行描述，后续在录制命令缓冲时(vkCmdBindVertexBuffers)，会绑定到 0 号槽位
    //vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);//第二个参数firstBinding = 0即表示 _vertexBuffer 真正“插”进了 0 号槽位，_vertexBuffer 绑定到了 0 号槽位；
    bindingDescription.binding = 0;//这套描述/规则，是专门写给第 0 号顶点缓冲区的
    bindingDescription.stride = sizeof(Vertex);//步长，每次读取数据的步长
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
std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {

    //VkVertexInputAttributeDescription通过binding连接缓冲区，通过location连接着色器中的变量
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    {
        VkVertexInputAttributeDescription attributeDescription;
        //attributeDescription.binding表示从哪块缓冲区读取数据
        attributeDescription.binding = 0;//即上面VkVertexInputBindingDescription描述中，索引0的内存数据
        // attributeDescription.location表述数据传递给着色器中的location = x的变量
        attributeDescription.location = 0;//这个就是vertexShader--layout(location = 0)
        attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescription.offset = offsetof(Vertex, _pos);
        attributeDescriptions.push_back(attributeDescription);
    }

    {
        VkVertexInputAttributeDescription attributeDescription;
        //attributeDescription.binding表示从哪块缓冲区读取数据
        attributeDescription.binding = 0;//即上面VkVertexInputBindingDescription描述中，索引0的内存数据
        // attributeDescription.location表述数据传递给着色器中的location= x的变量
        attributeDescription.location = 1;//这个就是vertexShader--layout(location = 1)
        attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescription.offset = offsetof(Vertex, _color);
        attributeDescriptions.push_back(attributeDescription);
    }

    {
        VkVertexInputAttributeDescription attributeDescription;
        //attributeDescription.binding表示从哪块缓冲区读取数据
        attributeDescription.binding = 0;//即上面VkVertexInputBindingDescription描述中，索引0的内存数据
        // attributeDescription.location表述数据传递给着色器中的location= x的变量
        attributeDescription.location = 2;//这个就是vertexShader--layout(location = 1)
        attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescription.offset = offsetof(Vertex, _texCoord);
        attributeDescriptions.push_back(attributeDescription);
    }

    return attributeDescriptions;
}

VulkanModel::VulkanModel(
    std::shared_ptr<VulkanContext> vkContext,
    std::shared_ptr<VulkanCommandManager> commandManager,
    const std::string& modelFilePath
)
{
    this->_vkContext = vkContext;
    this->_vkCommandManager = commandManager;
    this->_modelFilePath = modelFilePath;
    this->loadModel();
}

VulkanModel::~VulkanModel()
{
}

void VulkanModel::loadModel()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, this->_modelFilePath.c_str())) {
        throw std::runtime_error(err);
    }
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {//index表示一个顶点（Vertex）
            Vertex vertex{};
            //index.vertex_index代表的是当前这一个顶点的位置数据（Position）在全局顶点数组（attrib.vertices）中的索引（编号）。
            //attrib.vertices 是一个扁平的一维 float 数组。所有的顶点坐标在里面是这样挨个存放的：[x0, y0, z0, x1, y1, z1, x2, y2, z2, ...]
            vertex._pos = {
                attrib.vertices[3 * index.vertex_index + 0],//三个坐标分量
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            vertex._texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0 - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            //确保顶点去重
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(this->_vertices.size());
                this->_vertices.push_back(vertex);
            }
            this->_indices.push_back(uniqueVertices[vertex]);
        }
    }
    this->createVertexBuffers();
    this->createIndexBuffers();
}

void VulkanModel::createVertexBuffers()
{
    if (this->_vertices.size() > 0)
    {
        //暂存缓冲区
        VkDeviceSize bufferSize = sizeof(this->_vertices.at(0)) * this->_vertices.size();
        VulkanBuffer vulkanBufferStage(
            this->_vkContext,
            this->_vkCommandManager,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vulkanBufferStage.setBufferData(this->_vertices.data(), static_cast<size_t>(bufferSize), 0.0);

        //真正的顶点缓冲区
        this->_vertexBuffer = std::make_shared<VulkanBuffer>(
            this->_vkContext,
            this->_vkCommandManager,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vulkanBufferStage.copyToBuffer(this->_vertexBuffer->getBuffer(), bufferSize);
    }
}

void VulkanModel::createIndexBuffers()
{
    if (this->_indices.size() > 0)
    {
        //暂存缓冲区
        VkDeviceSize bufferSize = sizeof(this->_indices.at(0)) * this->_indices.size();
        VulkanBuffer vulkanBufferStage(
            this->_vkContext,
            this->_vkCommandManager,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vulkanBufferStage.setBufferData(this->_indices.data(), static_cast<size_t>(bufferSize), 0.0);

        //真正的顶点缓冲区
        this->_indexBuffer = std::make_shared<VulkanBuffer>(
            this->_vkContext,
            this->_vkCommandManager,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vulkanBufferStage.copyToBuffer(this->_indexBuffer->getBuffer(), bufferSize);
    }
}

// 将顶点缓冲和索引缓冲绑定到当前的 CommandBuffer 上
void VulkanModel::bindBuffer(VkCommandBuffer commandBuffer)
{
    VkBuffer vertexBuffers[] = { this->_vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = { 0 };
    uint32_t firstBinding = 0;
    uint32_t bindingCount = 1;
    //vertexBuffers缓冲区插进0号“槽位”
    vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, vertexBuffers, offsets);
    VkDeviceSize offset = 0;
    vkCmdBindIndexBuffer(commandBuffer, this->_indexBuffer->getBuffer(), offset, VK_INDEX_TYPE_UINT32);
}

// 发出绘制指令（vkCmdDraw）
void VulkanModel::draw(VkCommandBuffer commandBuffer)
{
    /**
    * VkCommandBuffer                             commandBuffer,
    * uint32_t                                    vertexCount,
    * uint32_t                                    instanceCount,
    * uint32_t                                    firstVertex,
    * uint32_t                                    firstInstance
    */
    vkCmdDraw(commandBuffer, this->_vertices.size(), 1, 0, 0);
}


// 发出绘制指令（vkCmdDrawIndexed）
void VulkanModel::drawIndex(VkCommandBuffer commandBuffer)
{
    // 发出索引绘制指令 (vkCmdDrawIndexed)
    // 参数1: 录制命令的 commandBuffer
    // 参数2: 索引总数 (_indexCount)
    // 参数3: 实例数量 (我们只画一个模型实体，所以是 1。如果要画森林，这就是树的数量)
    // 参数4: firstIndex (从哪个索引开始画，通常是 0)
    // 参数5: vertexOffset (为所有索引加上一个基础偏移，通常是 0)
    // 参数6: firstInstance (实例偏移，通常是 0)

    /**
    * VkCommandBuffer                             commandBuffer,
    * uint32_t                                    indexCount,
    * uint32_t                                    instanceCount,
    * uint32_t                                    firstIndex,
    * int32_t                                     vertexOffset,
    * uint32_t                                    firstInstance
    */
    vkCmdDrawIndexed(
        commandBuffer,
        static_cast<uint32_t>(this->_indices.size()),
        1,0,0,0);
}

#if 0
// 伪代码推演：未来极其简洁的渲染循环
void VulkanRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {

    // ... begin RenderPass ...
    // ... 绑定管线 pipeline->bind(cmd) ...
    // ... 绑定描述符集 (UBO和Texture) ...
    // 【魔法发生在这里】：您再也不需要关心底层的 VkBuffer 句柄和 offsets 数组了！
    _vikingRoomModel->bind(cmd);
    _vikingRoomModel->draw(cmd);
    // ... end RenderPass ...
}
#endif // 0