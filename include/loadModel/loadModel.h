#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional>
#include <vector>
#include <array>
#include"glm/vec2.hpp"
#include"glm/vec3.hpp"
#include"glm/mat4x4.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES//强制vec2/vec4对其
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

//队列族
struct QueueFamilyIndices {
    //uint32_t graphicsFamily;
    std::optional<uint32_t> _graphicsFamily;//支持图形绘制指令的队列族
    std::optional<uint32_t> _presentFamily;//支持呈现的队列族,确保设备可以在我们创建的表面上显示图像
    bool isComplete()
    {
        return this->_graphicsFamily >= 0 && this->_presentFamily >= 0;
    }
};

//交换链信息
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;//基本表面功能（交换链中图像的最小/最大数量，图像的最小/最大宽度和高度）
    std::vector<VkSurfaceFormatKHR> formats;//表面格式（像素格式、色彩空间）eg:R8B8G8A8
    std::vector<VkPresentModeKHR> presentModes;//可用的演示模式（立即模式/双缓冲垂直同步/三缓冲）
};

//顶点信息
struct Vertex {
    glm::vec3 _pos;
    glm::vec3 _color;
    glm::vec2 _texCoord;
    //顶点缓冲区层面信息-它告诉 GPU：从哪个内存缓冲区读取，读取的步长是多少
    static VkVertexInputBindingDescription getBindingDescription();
    //“属性”层面信息-它对应着顶点着色器（GLSL）中 layout(location = x) in vec3 pos; 里的 x
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    bool operator==(const Vertex& other) const;
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex._pos) ^
                (hash<glm::vec3>()(vertex._color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex._texCoord) << 1);
        }
    };
}

//ubo
struct UniformBufferObject {
    glm::mat4 _model;
    glm::mat4 _view;
    glm::mat4 _proj;
};

class HelloTriangleApplication {
public:
    void run();
private:
    void initVulkan();
    void mainLoop();
    void cleanup();
    void initWindow();
    void createInstance();
    void setupDebugMessenger();//校验层回调函数
    void pickPhysicalDevice();//物理设备
    void createLogicDevice();//逻辑设备--队列族
    void createSurface();//窗口表面

    GLFWwindow* _glfwWindow = nullptr;
    VkInstance _vkInstance;
    /*VkApplicationInfo（optional）应用程序信息 - 这些信息的填写不是必须的，\
    但填写的信息可能会作为驱动程序的优化依据，让驱动程序进行一些特殊的优化。\
    比如，应用程序使用了某个引擎，驱动程序对这个引擎有一些特殊处理，这时就可能有很大的优化提升。
    */
    static VkApplicationInfo appInfo;
    static VkInstanceCreateInfo vkInstanceCreateInfo;//（require）vulkan驱动程序需要使用的全局扩展和校验层
    VkDebugUtilsMessengerEXT _debugMessenger;//存储回调函数信息-启用校验层以后需要设置回调函数来获得回调信息
    //创建VkDebugUtilsMessengerEXT//存储回调函数信息-启用校验层以后需要设置回调函数来获得回调信息
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    //销毁VkDebugUtilsMessengerEXT
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    //物理设备
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    //选择物理设备
    VkPhysicalDeviceProperties _physicalDeviceProperties = VkPhysicalDeviceProperties();
    VkPhysicalDeviceFeatures _physicalDeviceFeatures = VkPhysicalDeviceFeatures();
    //对设备进行加权打分
    int rateDeviceSuitability(VkPhysicalDevice device);
    //队列族
    QueueFamilyIndices _physicalQueueFamilyIndices;
    //队列族
    bool isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    //逻辑设备
    VkDevice _logicDevice = nullptr;
    //指定创建的队列
    std::vector<VkDeviceQueueCreateInfo> _logicDeviceQueueCreateInfos;
    //创建逻辑设备
    static VkDeviceCreateInfo _vkLogicDeviceCreateInfo;
    /*获取队列句柄--创建逻辑设备时指定的队列会随着逻辑设备一同被创建，
    为了方便,我们添加了一个VkQueue成员变量来直接存储逻辑设备的队列句柄
    */
    //绘制指令队列句柄，创建逻辑设备会自动创建队列，需要添加句柄用于进行交互
    VkQueue _graphicsDrawQueue;
    //呈现指令队列句柄
    VkQueue _presentQueue;

    /*窗口表面--用于显示渲染后的图像
    --VK_KHR_surface--窗口表面扩展
    --VK_KHR_win32_surface--windows系统特有扩展
    --vkCreateWin32SurfaceKHR--windows
    --vkCreateXSurfaceKHR--Linux
    --glfw库--glfwCreateWindowSurface函数统一了各平台操作
    */
    VkSurfaceKHR _vkSurface;

    //交换链--本质上一个包含了若干等待呈现的图像的队列
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
    //1-选择表面格式（颜色深度等）R8B8G8A8
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    //2-选择演示模式（立即模式/双缓冲垂直同步/三缓冲模式）
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    //3-交换范围--交换链中图像的分辨率
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    //创建交换链
    void createSwapChain();

    VkSwapchainKHR _swapChain;

    //交换链图像格式- eg:RGBA8
    VkFormat _swapChainImageformat;

    //-交换范围--交换链中图像的分辨率
    VkExtent2D _swapChainExtent;

    //获取交换链中各个元素VkImage的句柄
    std::vector<VkImage> _swapChainImages;

    //针对每个VkImage，都需要创建VKImageView，描述了如何访问图像以及要访问图像的哪个部分
    std::vector<VkImageView> _swapChainImageViews;

    //创建图形管线
    void createGraphicsPipeline();
    VkPipeline _graphicsPipeline;
    
    //创建着色器模块
    VkShaderModule createShaderModule(const std::vector<char>& code);
    //渲染通道
    VkRenderPass _renderPass;
    void createRenderPass();

    //帧缓冲区--为每个VkImageView创建对应的缓冲区
    std::vector<VkFramebuffer> _swapChainFramebuffers;
    void createFramebuffers();
    
    //命令池-用于存储命令缓冲区
    VkCommandPool _commandPool;
    //创建命令池
    void createCommandPool();
    //命令缓冲区--可以同时处理多帧
    std::vector<VkCommandBuffer> _commandBuffers;
    //分配命令缓冲区--从命令池中分配单个命令缓冲区
    void createCommandBuffers();
    //记录命令缓冲区--将要执行的命令写入命令
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    //渲染帧
    void drawFrame();
    //当前帧
    uint32_t _currentFrame = 0;

    //同步对象--定义多重同步对象，可以处理多帧
    //信号量--用于GPU，栅栏--用于CPU
    std::vector<VkSemaphore>_imageAvailableSemaphores;//已从交换链获取图像并准备好渲染
    std::vector <VkSemaphore> _renderFinishedSemaphores;//渲染已完成并可以进行展示
    std::vector <VkFence> _inFlightFences;
    void createSyncObjects();

public:
    //窗口大小是否发生了变化
    bool _framebufferResized = false;
    //重建交换链--比如窗口大小发生变化，需要重置交换链
    void reCreateSwapChain();
    //销毁之前的交换链
    void cleanupSwapChain();

    void createVertexBuffer();

    //内存类型
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    //抽象缓冲区
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    //复制缓冲区
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    //索引缓冲区
    void createIndexBuffer();

    //描述符（Descriptor）:如果想传一些所有顶点共用的全局数据（比如相机的投影矩阵、模型的位置矩阵、或者一张贴图），不能把它塞进顶点里，需要用到描述符（Descriptor）
   //描述符布局
    void createDescriptorSetLayout();
    VkDescriptorSetLayout _descriptorSetLayout;
    VkPipelineLayout _pipelineLayout;


    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage);
    //描述符池-跟命令池类似
    VkDescriptorPool _descriptorPool;
    //描述符句柄
    std::vector<VkDescriptorSet> _descriptorSets;
    void createDescriptorPool();
    void createDescriptorSets();

    //纹理图像
    void createImage(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkSampleCountFlagBits numSamples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory);

    void generateMipmaps(
        VkImage image, 
        VkFormat imageFormat,
        int32_t texWidth, 
        int32_t texHeight, 
        uint32_t mipLevels);
    
    void createTextureImage();


    //图像布局转换
    void transitionImageLayout(
        VkImage image,
        VkFormat format, 
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t mipLevels);
    //复制图像
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    //纹理采样器
    void createTextureSampler();

    //查找支持格式VkFormat
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    void createDepthResources();
    //获取最大采样样本
    VkSampleCountFlagBits getMaxUsableSampleCount();
    //多重采样颜色缓冲区
    void createColorResources();

private:
    //抽象函数
    //创建/分配并开始记录命令缓冲区
    VkCommandBuffer beginSingleTimeCommands();
    //命令缓冲区记录结束
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    VkImageView createImageView(
        VkImage image, 
        VkFormat format, 
        VkImageAspectFlags aspectFlags,
        uint32_t mipLevels);



    //顶点属性
    /**
        const std::vector<Vertex> _vertices = {
        {{-0.5f, -0.5f,0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f,0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f,0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, 0.5f,0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},


         {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };

        const std::vector<uint16_t> _indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
        };
    */

    //顶点属性
    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;

    //创建顶点缓冲区
    VkBuffer _vertexBuffer;
    VkDeviceMemory _vertexBufferMemory;

    VkBuffer _indexBuffer;
    VkDeviceMemory _indexBufferMemory;

    //ubo
    std::vector<VkBuffer> _uniformBuffers;
    std::vector<VkDeviceMemory> _uniformBuffersMemory;
    std::vector<void*> _uniformBuffersMappedData;

    //texture
    VkImage _textureImage;//图像
    VkDeviceMemory _textureImageMemory;//内存
    VkImageView _textureImageView;//图像视图
    //纹理采样器
    VkSampler _textureSampler;
    //mimap相关
    uint32_t _mipLevels;


    //深度缓存相关
    VkImage _depthImage;//图像
    VkDeviceMemory _depthImageMemory;//内存
    VkImageView _depthImageView;//图像视图

    //多重采样相关-多重采样缓冲区--需要存储每个像素所需的采样数
    /**
    * 如果你不开启 MSAA，片段着色器会直接把颜色画到交换链图像上，然后直接显示。
    * 开启 MSAA 后，流程变成了：
    * 1-片段着色器将高精度的颜色数据（带多个采样点）渲染到这个函数创建的多重采样缓冲区中。
    * 2-Render Pass 结束前，执行一个 Resolve（解析）操作，把多重采样缓冲区的像素混合、抗锯齿化后，写入普通的交换链图像。
    * 3-多重采样缓冲区的数据被丢弃（因为它有 Transient 标志）。
    * 4-交换链图像被送到屏幕显示
    */
    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImage _multiSampleColorImage;
    VkDeviceMemory _multiSampleColorImageMemory;
    VkImageView _multiSampleColorImageView;


    /**
    * Note:在 Vulkan 中，VkBuffer 和 VkImage 确实就是一种类似 ID 或句柄（Handle）的纯逻辑对象。
    * 当你调用 vkCreateBuffer 或 vkCreateImage 时，Vulkan 驱动绝对不会为你分配任何用于存储像素或顶点数据的显存。
    * 它们的作用仅仅是向驱动描述一段元数据（Metadata），相当于一张“蓝图”或“空壳”，比如：
    * VkBuffer：记录了需要多大的空间、这个缓冲区是用来做顶点缓冲还是统一缓冲（Usage Flags）等。
    * VkImage：记录了图像的宽宽高、像素格式（Format）、是 2D 还是 3D、是否需要生成 Mipmap 等。
    * 真实的、用来存储数据的物理空间，全部是由 VkDeviceMemory 来分配和代表的
    * 
    * 
    * 标准的 Vulkan 资源创建流程:
    * 1-创建空壳：调用 vkCreateBuffer，得到一个纯逻辑的 VkBuffer 句柄。
    * 2-量尺寸：调用 vkGetBufferMemoryRequirements。因为不同 GPU 对内存的“对齐要求（Alignment）”不同，必须问 GPU：“为了装下我刚才描述的那个 Buffer，你需要什么样的内存？需要怎么对齐？”
    * 3-分配物理内存：调用 vkAllocateMemory 申请真实的 VkDeviceMemory（或者复用你之前早就申请好的大块内存池）。
    * 4-注入灵魂：调用 vkBindBufferMemory，告诉 GPU：“把这个 VkBuffer 句柄，绑定到这块 VkDeviceMemory 的特定偏移量（Offset）上”。
    * 直到第 4 步完成，这个 VkBuffer / VkImage 才有真实的物理存储，才可以被 GPU 读写。
    * 
    * 
    * 唯一常见的例外：Swapchain Image（交换链图像）
    * 当调用 vkGetSwapchainImagesKHR 获取用于直接显示到屏幕上的 VkImage 时，这些 Image 是由底层的操作系统窗口管理器（如 Windows 的 DWM、Linux 的 Wayland/X11）预先创建并分配好内存的。
    * 不需要也不能为它们分配或绑定 VkDeviceMemory。
    */

    void loadModel();
};