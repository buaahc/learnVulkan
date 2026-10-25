#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include <glm/gtx/hash.hpp>

// 前置声明
class VulkanContext;
class VulkanCommandManager;
class VulkanBuffer;

struct Vertex {
    glm::vec3 _pos;
    glm::vec3 _color;
    glm::vec2 _texCoord;

    // 获取绑定描述（告诉管线顶点数据有多宽，是一次读一个点还是一次读一个实例）
    //==============================================================
    static VkVertexInputBindingDescription getBindingDescription();

    // 获取属性描述（告诉管线 pos, color, texCoord 在数据块里的具体偏移量和格式）
     //==============================================================
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

    bool operator==(const Vertex& other) const {
        return _pos == other._pos && _color == other._color && _texCoord == other._texCoord;
    }
};

//哈希值：就是一个无序唯一值，可以理解每个Vertex数据生成一个类似身份证号的标识；
//定义哈希函数，为了让自定义结构体 Vertex 能够作为 std::unordered_map 的键（Key），进行顶点去重
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex._pos) ^
                (hash<glm::vec3>()(vertex._color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex._texCoord) << 1);
        }
    };
}

class VulkanModel {
public:
    // 构造函数：负责读取文件，去重顶点，并利用暂存缓冲(Staging)将其推入显卡
    VulkanModel(
        std::shared_ptr<VulkanContext> vkContext,
        std::shared_ptr<VulkanCommandManager> commandManager,
        const std::string& modelFilePath
    );

    ~VulkanModel();

    // 禁用拷贝语义（避免底层的 VulkanBuffer 被意外影响）
    VulkanModel(const VulkanModel&) = delete;
    VulkanModel& operator=(const VulkanModel&) = delete;

    // ----- 核心渲染接口 -----
    // 将顶点缓冲和索引缓冲绑定到当前的 CommandBuffer 上
    //==============================================================
    void bindBuffer(VkCommandBuffer commandBuffer);

    // 发出绘制指令（vkCmdDrawIndexed）vkCmdDraw
    //==============================================================
    void drawIndex(VkCommandBuffer commandBuffer);
   
    // 发出绘制指令（vkCmdDraw）
    //==============================================================
    void draw(VkCommandBuffer commandBuffer);

    std::shared_ptr<VulkanBuffer> getVertexBuffer()
    {
        return this->_vertexBuffer;
    }

    std::shared_ptr<VulkanBuffer> getIndexBuffer()
    {
        return this->_indexBuffer;
    }
private:
    std::shared_ptr<VulkanContext> _vkContext;
    std::shared_ptr<VulkanCommandManager> _vkCommandManager;
    std::string _modelFilePath;

    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;

    // 两个最重要的底层显存管家
    //=============================================
    std::shared_ptr<VulkanBuffer> _vertexBuffer;
    std::shared_ptr<VulkanBuffer> _indexBuffer;

    // 记录要画多少个索引（供 vkCmdDrawIndexed 使用）
    uint32_t _indexCount = 0;

    // 私有辅助方法
    void loadModel();

    void createVertexBuffers();
    void createIndexBuffers();
};