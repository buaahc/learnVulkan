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
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES//强制vec2/vec4对其
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


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
    glm::vec2 pos;
    glm::vec3 color;
    //顶点缓冲区层面信息-它告诉 GPU：从哪个内存缓冲区读取，读取的步长是多少
    static VkVertexInputBindingDescription getBindingDescription();
    //“属性”层面信息-它对应着顶点着色器（GLSL）中 layout(location = x) in vec3 pos; 里的 x
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

//ubo
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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

    //获取交换链中各个元素VkImage的句柄
    std::vector<VkImage> _swapChainImages;
    //交换链图像格式- eg:RGBA8
    VkFormat _swapChainImageformat;
    //-交换范围--交换链中图像的分辨率
    VkExtent2D _swapChainExtent;

    //针对每个VkImage，都需要创建VKImageView，描述了如何访问图像以及要访问图像的哪个部分
    std::vector<VkImageView> _swapChainImageViews;
    void createImageViews();

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
    //顶点属性
    const std::vector<Vertex> _vertices1 = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f,0.0f, 1.0f}}
    };

    const std::vector<Vertex> _vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
    };

    const std::vector<uint16_t> _indices = {
    0, 1, 2, 2, 3, 0
    };

    //创建顶点缓冲区
    VkBuffer _vertexBuffer;
    VkDeviceMemory _vertexBufferMemory;

    VkBuffer _indexBuffer;
    VkDeviceMemory _indexBufferMemory;

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

    std::vector<VkBuffer> _uniformBuffers;
    std::vector<VkDeviceMemory> _uniformBuffersMemory;
    std::vector<void*> _uniformBuffersMappedData;
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
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory);
    void createTextureImage();
    VkImage _textureImage;
    VkDeviceMemory _textureImageMemory;
    //图像布局转换
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    //复制图像
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    //图像视图
    VkImageView _textureImageView;
    void createTextureImageView();
    //纹理采样器
    VkSampler _textureSampler;
    void createTextureSampler();
private:
    //抽象函数
    //创建/分配并开始记录命令缓冲区
    VkCommandBuffer beginSingleTimeCommands();
    //命令缓冲区记录结束
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    VkImageView createImageView(VkImage image, VkFormat format);
};