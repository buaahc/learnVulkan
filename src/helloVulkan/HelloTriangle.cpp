#include<vector>
#include<map>
#include<set>


#define VK_USE_PLATFORM_WIN32_KHR
//#define GLFW_INCLUDE_VULKAN
//#include <GLFW/glfw3.h>
#include"HelloTriangle.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include "tools.h"
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

//校验层
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
    //"VK_LAYER_LUNARG_standard_validation"
};

//物理设备扩展，交换链扩展
const std::vector<const char*> physicalDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//检查交换链扩展
bool checkDeviceExtensionSupport(VkPhysicalDevice device);
//填充调试信使的创建信息-VkDebugUtilsMessengerCreateInfoEXT
void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif // DEBUG

std::vector<VkLayerProperties> availableLayers;
void HelloTriangleApplication::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void HelloTriangleApplication::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//不要创建OpenGL上下文
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    this->_glfwWindow = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void HelloTriangleApplication::initVulkan() {
    //类似于注册一家公司
    this->createInstance();
    //创建回调函数调试层-接收校验层信息
    this->setupDebugMessenger();
    //创建窗口呈现面-用于显示渲染后的图像-//租一块屏幕
    this->createSurface();
    //选择物理显卡-//招募一个画师
    this->pickPhysicalDevice();
    //激活显卡-逻辑设备-创建绘制队列_graphicsQueue/呈现队列_presentQueue句柄
    this->createLogicDevice();
    //创建交换链--多缓冲图像+呈现模式：绘制/等待呈现/呈现三步动态循环过程
    this->createSwapChain();
    //创建图像视图，如何访问交换链中的图像
    this->createImageViews();
    //创建渲染通道-渲染附件-子通道-就是对应openGL的renderPass
    this->createRenderPass();
    //提前烘焙的vulkan状态机（类似与openGL状态机，除了少量的动态状态外，渲染过程中几乎不允许修改）
    this->createGraphicsPipeline();
    //创建帧缓冲，将renderPass和vkImageView连接起来，renderPass即可绘制到vkImageView
    this->createFramebuffers();
    //创建命令池，用来管理命令缓冲区
    this->createCommandPool();
    //分配命令缓冲区-从命令池中分配一块给命令缓冲区
    this->createCommandBuffer();
    //创建同步对象-信号量/栅栏-信号里：gpu同步，栅栏：cpu等待gpu同步
    this->createSyncObjects();
}

void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(this->_glfwWindow)) {
        //处理事件输入
        glfwPollEvents();
        this->drawFrame();
    }
    vkDeviceWaitIdle(this->_logicDevice);
}

void HelloTriangleApplication::cleanup() {
    vkDestroySemaphore(this->_logicDevice, this->_imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(this->_logicDevice, this->_renderFinishedSemaphore, nullptr);
    vkDestroyFence(this->_logicDevice, this->_inFlightFence, nullptr);

    vkDestroyCommandPool(this->_logicDevice, this->_commandPool, nullptr);
    for (auto framebuffer : this->_swapChainFramebuffers) {
        vkDestroyFramebuffer(this->_logicDevice, framebuffer, nullptr);
    }

    vkDestroyPipeline(this->_logicDevice, this->_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(this->_logicDevice, this->_pipelineLayout, nullptr);
    vkDestroyRenderPass(this->_logicDevice, this->_renderPass, nullptr);
    //销毁图像视图
    for (auto imageView : this->_swapChainImageViews) {
        vkDestroyImageView(this->_logicDevice, imageView, nullptr);
    }
    //销毁交换链
    vkDestroySwapchainKHR(this->_logicDevice, this->_swapChain, nullptr);
    //销毁逻辑设备
    vkDestroyDevice(this->_logicDevice, nullptr);
    //销毁调试层
    if (enableValidationLayers)
        this->DestroyDebugUtilsMessengerEXT(this->_vkInstance, this->_debugMessenger, nullptr);
    //销毁窗口表面
    vkDestroySurfaceKHR(this->_vkInstance, this->_vkSurface, nullptr);
    vkDestroyInstance(this->_vkInstance, nullptr);
    glfwDestroyWindow(this->_glfwWindow);
    glfwTerminate();
}


//Vulkan 本身是一个平台无关的 API，这意味着它只负责图形渲染，不知道如何与特定操作系统（如 Windows, macOS, Linux）的窗口系统进行交互。
// 要在窗口中显示画面，我们需要启用特定的窗口扩展（Extensions），glfw扩展。
std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;

    // 1. 获取 GLFW 要求的 Vulkan 窗口扩展
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // 2. 将 C 风格的数组转换为 C++ 的 std::vector
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // 3. 如果启用了验证层（Validation Layers），则添加调试工具扩展，这个扩展允许你设置回调函数，以便在 Vulkan 发生错误或警告时打印出详细的调试信息
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

/**********************************校验层validationLayer****************/
//检查是否支持校验层
bool checkValidationLayerSupport() {
    uint32_t layerCount;
    //负责统计并返回当前操作系统中注册过的所有 Vulkan 层的信息
    //请求这台电脑上当前都安装了哪些 Vulkan 全局层？
    //得到所有的全局图层，并检查校验层是否可用
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    availableLayers.resize(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }
    }
    return true;
}

VkApplicationInfo HelloTriangleApplication::appInfo = VkApplicationInfo();
VkInstanceCreateInfo HelloTriangleApplication::vkInstanceCreateInfo = VkInstanceCreateInfo();

//关键步骤一：createIntance
void HelloTriangleApplication::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    //创建实例之前，检索并得到当前显卡驱动支持的Vulkan所有扩展列表，得到实例级的扩展列表
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> vulkanAllExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, vulkanAllExtensions.data());
    std::cout << "available extensions:\n";
    for (const auto& extension : vulkanAllExtensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    HelloTriangleApplication::appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    HelloTriangleApplication::appInfo.pApplicationName = "Hello Triangle";
    HelloTriangleApplication::appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    HelloTriangleApplication::appInfo.pEngineName = "No Engine";
    HelloTriangleApplication::appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    HelloTriangleApplication::appInfo.apiVersion = VK_API_VERSION_1_0;//指定使用vulkan1.0版本

    HelloTriangleApplication::vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    HelloTriangleApplication::vkInstanceCreateInfo.pApplicationInfo = &HelloTriangleApplication::appInfo;

    //为了能在屏幕上显示窗口并且方便找Bug，需要开启哪些扩展（Extensions）”
    //vulkan是平台无关的API，所以需要一个和窗口系统交互的扩展，glfw库包含了一个可以返回这一扩展的函数，可以直接使用它
    //VK_EXT_DEBUG_UTILS扩展，为了与窗口进行调试回调
    std::vector<const char*> glfwExtensions = getRequiredExtensions();//得到glfw窗口扩展+VK_EXT_DEBUG_UTILS调试扩展
    /**
    * 注意：我们得到程序所需的glfw窗口扩展+VK_EXT_DEBUG_UTILS调试扩展后，应该与上面vulkanAllExtensions，进行比对，看是否都支持
    */
    //挂载扩展层
    HelloTriangleApplication::vkInstanceCreateInfo.enabledExtensionCount = glfwExtensions.size();
    HelloTriangleApplication::vkInstanceCreateInfo.ppEnabledExtensionNames = glfwExtensions.data();

    /**
    * Vulkan 是一个底层的 API，为了追求极致的性能，它默认不会进行任何错误检查。
    * 如果开发者传错了参数，程序通常会直接崩溃而没有任何提示。
    * 为了解决这个问题，Vulkan 引入了校验层机制，可以在开发阶段开启错误检查和调试信息的输出。
    */
    /**
    * 临时调试信使--仅用于监控createInstance / destoryInstance阶段校验层的校验,
    * 真正的调试信使会在后面使用setupDebugMessenger进行创建，但是创建正式的调试信使需要vkInstance，
    * 然后销毁正式信使又必须在销毁vkInstance之前，所以在创建和销毁vkInstance阶段，不能使用正式信使，因为还没创建或者已经销毁，
    * 所以需要这个临时调试信使来校验创建和销毁vkInstance阶段是否出错
    */
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers)//校验层，创建实例时添加校验层信息
    {
        HelloTriangleApplication::vkInstanceCreateInfo.enabledLayerCount = validationLayers.size();
        HelloTriangleApplication::vkInstanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        //调试信使--填充调试信使的配置信息，告诉 Vulkan 你希望接收什么级别的调试信息、什么类型的调试信息，以及接收到信息后该交给哪个函数处理。
        populateDebugMessengerCreateInfo(debugCreateInfo);
        HelloTriangleApplication::vkInstanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        HelloTriangleApplication::vkInstanceCreateInfo.enabledLayerCount = 0;
        HelloTriangleApplication::vkInstanceCreateInfo.pNext = nullptr;
    }
    if (vkCreateInstance(&HelloTriangleApplication::vkInstanceCreateInfo, nullptr, &this->_vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

//关键步骤二：创建调试信使VkDebugUtilsMessengerEXT--_debugMessenger--setupDebugMessenger()
//2.1-回调函数--接收校验层信息，函数的第一个参数messageSeverity指定了消息的级别
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,//消息级别
    VkDebugUtilsMessageTypeFlagsEXT messageType,//消息类型
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,//回调核心数据
    void* pUserData) //自定义传输的数据
{
    //回调核心数据
    // pCallbackData->pMessage: 一个以 null 结尾的字符串，里面是由校验层生成的具体的、人类可读的错误描述。
    // pCallbackData->pObjects: 导致这个错误的具体 Vulkan 对象数组（比如是哪个 Buffer 或 Image 出了问题）。
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        // Message is important enough to show
    }
    /**
    * 返回值：
    * 如果返回 VK_TRUE：意味着你告诉 Vulkan “这是一个致命错误，请立刻中断触发这个错误的 Vulkan 函数调用”。这通常会导致程序引发 VK_ERROR_VALIDATION_FAILED_EXT 异常并可能崩溃。
    * 如果返回 VK_FALSE：意味着“我已经收到这条信息了，请继续执行原定的 Vulkan 调用”。
    * 在绝大多数情况下，我们仅仅是想记录或打印错误，并不想强行中断 Vulkan 底层的执行流，因此总是返回 VK_FALSE。
    */
    return VK_FALSE;

}
//2.2-填充调试信使的配置信息VkDebugUtilsMessengerCreateInfoEXT，告诉 Vulkan 你希望接收什么级别的调试信息、什么类型的调试信息，以及接收到信息后该交给哪个函数处理。
void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    //指定回调函数处理的消息级别：
    // VERBOSE（详细信息/诊断信息）
    // WARNING（警告信息）
    // ERROR（错误信息）
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    //指定回调函数处理的消息类型：
    // GENERAL：发生了一些与规范或性能无关的事件，
    // VALIDATION：发生了违反 Vulkan 规范的情况（比如传错参数）。
    // PERFORMANCE：发生了可能会影响 Vulkan 运行性能的情况。
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    //指定回调函数-由谁来处理这些信息--通常会在这个回调函数里把错误信息通过 std::cout 或 printf 打印到屏幕上
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; // Optional--传递自定义的指针数据
}

//2.3-创建调试信使--VkDebugUtilsMessengerEXT--_debugMessenger//存储回调函数信息-启用校验层以后需要设置回调函数来获得回调信息
VkResult HelloTriangleApplication::CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

//2.4-销毁调试信使--VkDebugUtilsMessengerEXT--_debugMessenger
void HelloTriangleApplication::DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

//2.5-上层调用创建调试信使--根据回调函数信息VkDebugUtilsMessengerCreateInfoEXT createInfo，创建VkDebugUtilsMessengerEXT实例_debugMessenger
void HelloTriangleApplication::setupDebugMessenger() {
    if (!enableValidationLayers) return;
    //包含有关消息传递程序及其回调函数的详细信息
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    if (CreateDebugUtilsMessengerEXT(this->_vkInstance, &createInfo, nullptr, &this->_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

//关键步骤三：创建窗口表面
//建立Vulkan和操作系统窗口系统之间的“桥梁”——窗口表面（Window Surface）
//Vulkan本身是一个与平台无关的 API，它默认是在“幕后”画图的，根本不知道什么是“Windows 窗口”或“Mac 屏幕”,
//要想把Vulkan画好的图显示到你看得见的窗口上，就必须创建一个 VkSurfaceKHR（窗口表面）
void HelloTriangleApplication::createSurface()
{
#if 0//windows系统专属的窗口表面创建方式
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = glfwGetWin32Window(this->_glfwWindow);
    createInfo.hinstance = GetModuleHandle(nullptr);
    //vkCreateXcbSurfaceKHR
    if (vkCreateWin32SurfaceKHR(this->_vkInstance, &createInfo, nullptr, &this->_vkSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
#endif // 0//windows系统专属的窗口表面创建方式
    //glfw库--glfwCreateWindowSurface函数在不同平台的实现是不同的，可以跨平台使用
    if (glfwCreateWindowSurface(this->_vkInstance, this->_glfwWindow, nullptr, &this->_vkSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

//关键步骤四：选择物理显卡
//4.1-判断指定的显卡上是否支持绘制和呈现队列族，并且支持将图像输出到我们创建的窗口表面；
//这段代码的核心逻辑是：像查户口一样，遍历显卡上所有的“工作部门（队列族）”，找到一个能“画图”的部门，
// 再找到一个能“把图贴到屏幕上”的部门，把这两个部门的编号（Index）记下来，以备后用
QueueFamilyIndices HelloTriangleApplication::findQueueFamilies(VkPhysicalDevice physicalDevice) {
    // Logic to find queue family indices to populate struct with
    //首先获取设备的队列族个数
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        //1-查找图形绘制队列族-其实就是一个uint32_t类型索引index--找到支持VK_QUEUE_GRAPHICS_BIT绘制指令的队列族
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            this->_physicalQueueFamilyIndices._graphicsFamily = i;
        }

        //2-查找呈现队列族,查找支持将图像输出到我们创建的窗口表面_vkSurface上的队列族
        //“嘿，你能把图像输出到**这个窗口表面（this->_vkSurface）**上吗
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, this->_vkSurface, &presentSupport);
        if (presentSupport && queueFamily.queueCount > 0)
        {
            this->_physicalQueueFamilyIndices._presentFamily = i;
        }
        //注：在大多数现代显卡上，支持绘制的队列族通常也支持呈现，即这两个索引可能指向同一个队列族，但也有些设备会将它们分开，所以必须分别查询。
        //注意：绘制队列族和呈现队列族在独立显卡上一般是同一个index，这样效率最高，集成显卡或者低端显卡可能两个队列族的index不同
        if (this->_physicalQueueFamilyIndices.isComplete())
        {
            break;
        }

        i++;
    }
    return this->_physicalQueueFamilyIndices;
}

//4.2-得到物理显卡的扩展属性列表，并判断需要的扩展-主要判断交换链扩展是否在列表里面
bool checkDeviceExtensionSupport(VkPhysicalDevice device) {

    //得到显卡支持的所有扩展列表
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(physicalDeviceExtensions.begin(), physicalDeviceExtensions.end());

    //判断需要的扩展-交换链是否在显卡所支持的扩展列表里面，检查交换链是否可用
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

//4.3-查询尺寸能力（Capabilities）、色彩格式（Formats）、显示模式（Present Modes）
// 在准备创建“交换链（Swapchain）”之前，全面调查一下显卡（物理设备）和窗口（Surface）配合工作时，到底具备哪些能力和限制，
// 需要知道能画多大尺寸的画、用什么颜料、画画的速度有多快，呈现的模式是啥，
SwapChainSupportDetails HelloTriangleApplication::querySwapChainSupport(VkPhysicalDevice physicalDevice)
{
    SwapChainSupportDetails details;
    //1-查询基本能力，查询显卡在当前窗口表面上绘图的基础物理属性限制，
    // 交换链图像数量（通常2张，双缓冲）/分辨率限制/变换支持：当前是否支持旋转
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, this->_vkSurface, &details.capabilities);

    //2-查询显卡和窗口组合支持哪些像素颜色格式和色彩空间- VK_FORMAT_B8G8R8A8_SRGB
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->_vkSurface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->_vkSurface, &formatCount, details.formats.data());
    }

    //3-查询支持的显示模式，呈现模式
    //VK_PRESENT_MODE_IMMEDIATE_KHR：画完立刻显示。会导致画面撕裂（Tearing）。
    //VK_PRESENT_MODE_FIFO_KHR：相当于垂直同步（V - Sync）。画好的图排队等屏幕刷新，这是 Vulkan 唯一保证所有显卡都支持的模式。
    //VK_PRESENT_MODE_MAILBOX_KHR：三重缓冲。如果画得太快，新的图会直接替换掉队列里旧的图，既能保证低延迟，又不会画面撕裂。（通常是游戏的首选）。
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->_vkSurface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->_vkSurface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

//4.4-选择合适的显卡--绘制和呈现的队列族，支持将图像输出到创建的窗口表面_vkSurface，支持交换链扩展，查询显卡的相关能力需支持交换链的需求
bool HelloTriangleApplication::isDeviceSuitable(VkPhysicalDevice device) {
#if 0
    //查询设备的基本属性，例如名称、类型和支持的 Vulkan 版本
    vkGetPhysicalDeviceProperties(device, &this->_physicalDeviceProperties);
    //查询对纹理压缩、64 位浮点数和多视口渲染（对 VR 很有用）等可选功能的支持情况
    vkGetPhysicalDeviceFeatures(device, &this->_physicalDeviceFeatures);
    return this->_devicePropertie.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        this->_physicalDeviceFeatures.geometryShader;//支持几何着色器
#endif // 0

    //1-判断物理设备支持的队列族（绘制队列族，呈现队列族，支持将图像输出到我们创建的窗口表面）
    this->findQueueFamilies(device);

    //2-物理设备是否支持特定的扩展，目前是交换链扩展
    bool swapchainExtensionsSupported = checkDeviceExtensionSupport(device);

    //3-查询交换链支持的详细信息-交换链图像大小/数量/图像格式/图像呈现方式
    bool swapChainAdequate = false;
    if (swapchainExtensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    return this->_physicalQueueFamilyIndices.isComplete() && swapchainExtensionsSupported && swapChainAdequate;
}

//4.5-选择最终的物理显卡
void HelloTriangleApplication::pickPhysicalDevice() {
    //请求显卡列表，列出显卡信息
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(this->_vkInstance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(this->_vkInstance, &deviceCount, devices.data());

    //遍历所有的显卡，检查是否有合适的显卡
    for (const auto& device : devices) {
        //找到合适的显卡，就是找到绘制和呈现队列族
        if (isDeviceSuitable(device))
        {
            this->_physicalDevice = device;
            break;
        }
    }
    //对显卡按照支持的特性数量进行排序
#if 0
    // Use an ordered map to automatically sort candidates by increasing score
    std::multimap<int, VkPhysicalDevice> candidates;

    for (const auto& device : devices) {
        int score = rateDeviceSuitability(device);
        candidates.insert(std::make_pair(score, device));
    }

    // Check if the best candidate is suitable at all
    if (candidates.rbegin()->first > 0) {
        this->_physicalDevice = candidates.rbegin()->second;
    }
    else {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
#endif // 0

    if (this->_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

//对设备进行加权打分
int HelloTriangleApplication::rateDeviceSuitability(VkPhysicalDevice device) {

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (this->_physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    // Maximum possible size of textures affects graphics quality
    score += this->_physicalDeviceProperties.limits.maxImageDimension2D;

    // Application can't function without geometry shaders
    if (!this->_physicalDeviceFeatures.geometryShader) {
        return 0;
    }
    return score;
}

//关键步骤五：创建逻辑设备-激活显卡
VkDeviceCreateInfo HelloTriangleApplication::_vkLogicDeviceCreateInfo = VkDeviceCreateInfo();
void HelloTriangleApplication::createLogicDevice()
{
    //this->_queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    //this->_queueCreateInfo.queueFamilyIndex = this->_physicalQueueFamilyIndices.graphicsFamily.value();//之前创建的队列
    //this->_queueCreateInfo.queueCount = 1;
    //float queuePriority = 1.0f;
    //this->_queueCreateInfo.pQueuePriorities = &queuePriority;

    //1-指定要创建的逻辑队列（绘制/呈现），可以理解为传送带，物理队列族（绘制/呈现）关联到逻辑设备信息，std::set自动去重，如果_graphicsFamily和_presentFamily相同，std::set将保证只使用一个；
    std::set<uint32_t> uniqueQueueFamilies = { this->_physicalQueueFamilyIndices._graphicsFamily.value(), this->_physicalQueueFamilyIndices._presentFamily.value() };
    float queuePriority = 1.0f;
    for (uint32_t physicalQueueFamily : uniqueQueueFamilies) {
        //为每个索引创建一个 VkDeviceQueueCreateInfo 并存入 _logicDeviceQueueCreateInfos 向量中
        VkDeviceQueueCreateInfo logicDeviceQueueCreateInfo{};
        logicDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        logicDeviceQueueCreateInfo.queueFamilyIndex = physicalQueueFamily;
        logicDeviceQueueCreateInfo.queueCount = 1;
        //你必须给传送带定个优先级（0.0 到 1.0），这就好比告诉工厂：“这条传送带上的任务最重要（1.0），给我优先处理”。
        logicDeviceQueueCreateInfo.pQueuePriorities = &queuePriority;
        this->_logicDeviceQueueCreateInfos.push_back(logicDeviceQueueCreateInfo);//std::vector<VkDeviceQueueCreateInfo> _logicDeviceQueueCreateInfos;
    }

    this->_vkLogicDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    this->_vkLogicDeviceCreateInfo.pQueueCreateInfos = this->_logicDeviceQueueCreateInfos.data();//指向队列创建信息
    this->_vkLogicDeviceCreateInfo.queueCreateInfoCount = this->_logicDeviceQueueCreateInfos.size();

    //2-指定使用的设备特性
    //告诉Vulkan打算使用物理设备的哪些高级特性（例如几何着色器、多点采样、各向异性过滤等）,通常这些特性在之前的“挑选物理设备”阶段已经查询过了。
    this->_vkLogicDeviceCreateInfo.pEnabledFeatures = &this->_physicalDeviceFeatures;

    //3-启用交换链扩展--逻辑设备支持交换链扩展
    //逻辑设备本身不包含显示画面的功能，为了让显卡能把图像显示到 Windows/Linux 窗口上，必须启用特定的扩展，最常见的是 VK_KHR_swapchain（交换链扩展）。
    this->_vkLogicDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(physicalDeviceExtensions.size());
    this->_vkLogicDeviceCreateInfo.ppEnabledExtensionNames = physicalDeviceExtensions.data();

    //4-使用与vkInstance相同的校验层，创建逻辑设备添加校验层信息
    if (enableValidationLayers) {
        this->_vkLogicDeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        this->_vkLogicDeviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        this->_vkLogicDeviceCreateInfo.enabledLayerCount = 0;
    }
    //5-创建逻辑设备
    if (vkCreateDevice(this->_physicalDevice, &this->_vkLogicDeviceCreateInfo, nullptr, &this->_logicDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    //6-从逻辑设备上得到图形队列族的句柄，拿到向显卡发送命令的通道，有了逻辑设备和队列句柄，我们现在就可以真正开始使用显卡来执行任务了
    vkGetDeviceQueue(this->_logicDevice, this->_physicalQueueFamilyIndices._graphicsFamily.value(), 0, &this->_graphicsQueue);
    vkGetDeviceQueue(this->_logicDevice, this->_physicalQueueFamilyIndices._presentFamily.value(), 0, &this->_presentQueue);
}

//关键步骤六：创建交换链
/**
* 交换链本质上就是一组图像（Images）的队列，显卡负责在这些图像上画画，画好一张就送到屏幕上显示，屏幕显示完再还给显卡继续画，如此循环。
* 交换链的动态循环系统：绘制--等待呈现--呈现；
* 缓冲（几张图）和呈现模式不是一个概念:
* 三缓冲+垂直同步模式，A/B/C三张图严格按照绘制-等待呈现-呈现序列排队；
* 三缓冲+邮箱模式：展示引擎（显示器）正在呈现A，B绘制完毕后会放到等待呈现区，接下来绘制C，C绘制完毕后，如果A画面仍在呈现，那C将直接替换到等待呈现队列中的B，
* 下一次展示引擎将呈现C画面,B画面被丢弃了，而被丢弃的B画面会被重置为空闲队列，又被重新绘制。
*/
//6.1-选择表面格式（颜色深度等）R8B8G8A8-SRGB空间
VkSurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    //大多数情况下采用第一个指定的格式就足够
    return availableFormats[0];
}

//6.2-选择演示模式（立即模式/双缓冲垂直同步/三缓冲模式）
VkPresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) //三缓冲模式
        {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;//双缓冲-垂直同步
}

//6.3-选择交换范围--交换链中图像的分辨率
VkExtent2D HelloTriangleApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT_MAX/*std::numeric_limits<uint32_t>::max()*/)
    {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(this->_glfwWindow, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

//6.4-创建交换链
void HelloTriangleApplication::createSwapChain() {
    //1-查询尺寸能力（Capabilities）、色彩格式（Formats）、显示模式（Present Modes）
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(this->_physicalDevice);

    //2-选择颜色格式和色彩空间
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);

    //3-选择呈现模式--立即模式/双缓冲/三缓冲等
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);

    //4-选择交换链范围-分辨率
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    //交换链中需要多少张图像，实现中规定了正常运行所需的最小图像数量，且不超过最大数量
    //swapChainSupport.capabilities.minImageCount一般等于2
    //所以我们创建交换链一般使用三张图像,
    // 缓冲和呈现模式不是一个概念，缓冲就是使用几张图-imageCount，双缓冲（2张图）/三缓冲（3张图）
    // 呈现模式：立即模式/垂直同步（FIFO）/邮箱模式MailBox
    // 双缓冲/三缓冲垂直同步模式，A B C三幅图像绘制/呈现严格排队，
    // 邮箱模式下（必须是三缓冲），A画完呈现，B开始画，画完放到邮箱中等待，C开始画，如果C画完，将直接替换到邮箱中的B，显示器呈现完A后，将开始呈现C
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = this->_vkSurface;

    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;//每个图像包含的层次，除非在做 VR（双眼立体渲染需要2层）或者立方体渲染，否则通常都是1。
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//图像用途，把这些图像当作颜色附件，也就是直接往上面画图（渲染输出）


    uint32_t queueFamilyIndices[] = { this->_physicalQueueFamilyIndices._graphicsFamily.value(), this->_physicalQueueFamilyIndices._presentFamily.value() };

    //存在多个队列族时，如何处理交换链图形，队列族中的图形队列族与呈现队列族不同，将从图形队列绘制交换链中的图像，然后将其提交到表示队列
    if (this->_physicalQueueFamilyIndices._graphicsFamily != this->_physicalQueueFamilyIndices._presentFamily) {
        //图形队列族和显示队列族索引不同,使用并发模式
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        //图形队列族和显示队列族索引相同-独占模式-效率最高
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0; // Optional
        swapchainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
    }
    //是否对交换链中的图形执行特定的变换，例如旋转90度
    swapchainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    swapchainCreateInfo.presentMode = presentMode;
    //启用裁切，将不关心被遮挡像素的颜色，如果你的游戏窗口被另一个窗口（比如系统计算器）挡住了一部分，设为 VK_TRUE 表示不去计算被挡住的像素，可以大幅提升性能。
    swapchainCreateInfo.clipped = VK_TRUE;

    //使用 Vulkan 时，应用程序运行时交换链可能会失效或未优化，例如，由于窗口大小调整。
    // 在这种情况下，交换链实际上需要从头开始重新创建，
    // 因此必须在此字段中指定对旧交换链的引用。
    // 这是一个复杂的主题，我们将在以后的章节中详细学习。
    // 现在，我们假设只会创建一个交换链。
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    //创建交换链
    if (vkCreateSwapchainKHR(this->_logicDevice, &swapchainCreateInfo, nullptr, &this->_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    //得到交换链真实的图像句柄-_swapChainImages-VkImage
    vkGetSwapchainImagesKHR(this->_logicDevice, this->_swapChain, &imageCount, nullptr);
    //VkImage
    this->_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(this->_logicDevice, this->_swapChain, &imageCount, this->_swapChainImages.data());


    this->_swapChainExtent = extent;
    this->_swapChainImageformat = surfaceFormat.format;
}

// 关键步骤七：为每个VkImage创建imageView
/**
 *可以理解为vkImage创建使用说明书，为交换链中的每一张“原始画板”配上一双“眼睛”和一套“说明书”。
 *vkImage可以理解是一块资源，他可以仅包含一张图像（texture2D），也可以包含6张图像（cubeTexture），或者textureArray（可以包含N层图像）,
 *vkImageView可以理解为视图，如何使用这块资源
*/
void HelloTriangleApplication::createImageViews()
{
    this->_swapChainImageViews.resize(this->_swapChainImages.size());
    for (size_t i = 0; i < this->_swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = this->_swapChainImages[i];
        //因为要画的是 2D 屏幕画面，所以选择 2D，如果是VR或者是立方体贴图（Cube Map），这里会改成CUBE或2D_ARRAY。
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//1D/2D/3D/立方图纹理？
        createInfo.format = this->_swapChainImageformat;
        //components字段允许重新排列颜色通道--Swizzling（重组）
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        //subresourceRange字段描述了图像的用途以及需要访问图像的哪个部分
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//颜色目标
        createInfo.subresourceRange.baseMipLevel = 0;//不包含任何mipmapping层级或多层图像
        createInfo.subresourceRange.levelCount = 1;//不包含任何 mipmapping 层级或多层图像
        createInfo.subresourceRange.baseArrayLayer = 0;//从vkimage的第几层开始访问
        createInfo.subresourceRange.layerCount = 1;//能访问多少层
        if (vkCreateImageView(this->_logicDevice, &createInfo, nullptr, &this->_swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

//关键步骤八：创建渲染通道--帧缓冲区附件--RenderPass
/**
* Render Pass（项目手册）：
“我们要准备一张 A4 纸（附件）。”
“开始前要把纸擦干净（LoadOp Clear）。”
“结束后要把画好的纸装裱起来（StoreOp Store）。”
Subpass 0（步骤一：素描）：
“在纸上画出轮廓。”
“只能用铅笔。”
Subpass 1（步骤二：上色）：
“根据轮廓涂色。”
“可以直接看到步骤一留下的痕迹（Input Attachment）。”
VkPipeline（画笔工具）：
具体的画笔设置。注意： 每一个流水线（Pipeline）在创建时，都必须明确指定它属于哪一个 Render Pass 的哪一个 Subpass。
*/
void HelloTriangleApplication::createRenderPass()
{
    //设置当前renderPass需要的帧缓冲附件，此处并没有真正的创建帧缓冲，只是标识出renderPass需要哪些帧缓冲附件，
    //后面的vkframeBuffer才是真正的创建对应于当前renderPass的帧缓冲
    
    //1-列出帧缓冲附件（Attachment），通常指的就是帧缓冲中的图像（如颜色缓冲、深度缓冲）
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = this->_swapChainImageformat; // 必须与交换链图像格式一致
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;// 不使用多重采样 (MSAA)

    //loadOp/storeOp: 控制数据的读写。CLEAR 确保每一帧开始都是干净的，STORE 确保我们画的东西被保留下来。
    //颜色/深度缓冲区
    // 渲染前：清除之前的图像内容 (类似 glClear)
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //绘制新帧--渲染后的内容将存储在内存中，稍后可以读取
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    //模板缓冲区（这里没用到，所以设为 DONT_CARE）
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    //图像布局转换
    /**布局分为渲染开始前，渲染过程中，渲染结束后三种布局；
    * initialLayout	    UNDEFINED	                渲染通道开始前	告诉 GPU 丢弃旧数据，准备新一帧。
    * reference.layout	COLOR_ATTACHMENT_OPTIMAL	子通道运行中	告诉 GPU 此时是以最高效的写入模式在工作，这是一种为了“作为颜色缓冲区被写入”而极度优化的布局。
    * finalLayout	    PRESENT_SRC_KHR	            渲染通道结束后	转换成显示器能看懂的格式，准备展示。
    */
    /**Layout (布局): Vulkan 为了优化性能，图像在不同用途下会有不同的内存排列方式。
    * 设置initialLayout = VK_IMAGE_LAYOUT_UNDEFINED，GPU 会认为图像里的旧数据是没用的，它可以为了优化性能直接“丢弃”旧内容。
    * 这非常适合我们要清除(Clear) 屏幕的情况。既然我们要重新画，那么图像之前的状态（上一次渲染剩下的垃圾数据）确实不重要。
    * 设置finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 呈现布局：意味着渲染结束后，图像将直接交给显示器显示，这是一种专门为了“显示到屏幕”而优化的布局（Present Source）
    * 只有当图像要通过“交换链(Swapchain)”交给显示器显示时，才使用这个布局，当渲染通道结束时，Vulkan 会自动将图像从渲染时的状态转换到这个布局，以便显示引擎读取。
    */
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 渲染开始前布局：不关心图像在内存中的布局
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;// 渲染结束后布局：转换为适合交换链呈现的格式//交换链中要展示的图像

    //2-定义附件引用--每个附件都需要定义一个附件引用
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;//使用索引为0的附件，即上面定义的VkAttachmentDescription colorAttachment
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//渲染过程中：引用的附件用作何种布局--这是一种为了“作为颜色缓冲区被写入”而极度优化的布局。

    //3-子通道：渲染通道renderPass代码执行之处,是渲染命令真正执行的地方
    /**
    * 子通道可以理解为OpenGL的渲染pass，vulkan为了效率引入子通道概念，一个渲染通道可以包含一个或多个子通道，
    * 传统方式：如果你分两个渲染通道（Render Pass A 和 B），GPU 需要把第一步的结果写回显存（VRAM），第二步再从显存读回来，显存带宽非常昂贵且慢。
    * 多子通道方式：在一个渲染通道内定义两个子通道。GPU 可以利用** 高速片上缓存（On - chip memory） * *直接把第一个子通道的结果传给第二个，不需要经过显存。这在“延迟渲染”（Deferred Shading）中能极大提升性能。
    */
    VkSubpassDescription subpass{};
    //指定为图形子通道--因为也可能为计算子通道
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    //指定对颜色附件的引用-也可以引用以下其他类型的附件
    /**
        pInputAttachments从着色器读取的附件
        pResolveAttachments用于多重采样颜色附件的附件
        pDepthStencilAttachment：深度和模板数据的附件
        pPreserveAttachments：此子通道未使用的附件，但其数据必须保留
    */
    subpass.pColorAttachments = &colorAttachmentRef;//子通道引用附件，索引为0，对应layout(location = 0)
    subpass.pDepthStencilAttachment;//子通道深度附件


    //4-子通道依赖项--它负责处理图像的读写同步
    //GPU 的渲染像流水线（Pipeline）一样分为很多阶段（读取顶点 -> 顶点着色器 -> ... -> 颜色输出）。
    /**
    * 它的作用是：控制“谁先做完，谁才能开始”。
    * 因为GPU是并行工作的，如果你不显式告诉它顺序，它可能会在上一帧图像还没显示完时，就开始往这张图里写新的数据，导致画面撕裂或崩溃。
    * 下面代码的结果是：子通道dstSubpass会在运行到VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT阶段后，一直等待srcSubpass运行到VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT阶段释放，dstSubpass子通道开始写入，
    * 这段代码翻译成白话就是：“嘿，GPU！在交换链把图像读完（输出阶段）之前，我的子通道 0 绝对不准往颜色缓冲区里写一个像素！”
    */
    VkSubpassDependency dependency{};
    //4.1-确定参与者：谁等谁？srcSubpass可以理解为要被覆盖的缓存（源因子），dstSubpass可理解为准备要画的动作（目标），dstSubpass等待srcSubpass
     //下面两句意思是：我们要处理的是“渲染通道外部”与“内部子通道 0”之间的先后顺序。
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;//指渲染通道（Render Pass）之外的操作，在这里它特指“交换链（Swapchain）正在读取图像并准备显示到屏幕”的操作（即呈现）。
    dependency.dstSubpass = 0;//指的是我们代码里定义的第一个子通道

    //4.2-确定时间点：在哪个阶段等待？
    //srcStageMask/dstStageMask：表示依赖者dstSubpass需要在运行到dstStageMask阶段进行等待，等待被依赖者srcSubpass运行完srcStageMask阶段并发出信号，依赖者dstSubpass收到信号才能继续运行；
    //srcAccessMask/dstAccessMask：srcAccessMask表示被依赖者dstSubpass在运行到srcStageMask后做什么，即在发出信号之前做什么，而dstAccessMask表示依赖者dstSubpass接收到信号后做什么
    //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT含义：我们要等待“外部”执行到颜色附件输出阶段 (Color Attachment Output)。也就是等交换链完成对图像的读取，正准备释放它的那一刻。
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;//在渲染通道开始前，我们不关心外部具体的内存访问状态（通常因为交换链的操作由信号量处理，这里设为 0 是安全的）

    //dstStageMask: 我方（目的）运行到哪一步时，必须停下来等对方？
    //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT含义：我们的子通道在开始写颜色之前（即 Color Attachment Output 阶段），必须原地待命，不能越界。
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    //4.3-不仅要控制时间（Stage），还要控制内存的访问权限（Access）
    //明确告诉 GPU，我们的子通道接下来的操作是 写入 (WRITE) 颜色附件，在对方完成输出之前，我不准执行任何“写入颜色”的操作。
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //5-创建渲染通道
    //附件和引用它的子通道都已描述完毕，开始创建渲染通道
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    //把所有的附件描述放进一个数组std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    renderPassInfo.pAttachments = &colorAttachment;//附件数组--对应上面定义的附件，由于只有一个附件，直接赋值指针即可
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(this->_logicDevice, &renderPassInfo, nullptr, &this->_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

//关键步骤九：创建图形管线vkPipeline-每个vkPipeline对应一个subPass（真正代码执行的地方）,vkPipeline就是在绘制之前把所有的状态设置好，绘制过程中几乎不允许修改-可以理解为vulkan状态机
/**
* 在 Vulkan 中，图形管线是一个极其庞大的状态机,与OpenGL可以在绘制时随时改变状态（如随时开关深度测试、改变混合模式）不同，
* Vulkan 要求你在绘制前把几乎所有的状态（着色器、混合模式、光栅化设置等）提前打包“烘焙”成一个不可变的对象，绘制过程中不可修改，
* 这就是图形管线（VkPipeline）,这样做是为了最大化 GPU 的运行效率。
* 渲染管线就是为subpass提前设置好的状态机（状态机快照），在绘制模型时绑定（启动）渲染管线，那这些状态都会起作用，然后绘制模型（可以理解为opengl状态机，但是几乎不允许实时修改，仅少量的动态状态可修改）
*/
//9.1-着色器相关--读取着色器字节码信息，保存在VkShaderModule
VkShaderModule HelloTriangleApplication::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(this->_logicDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

//9.2-创建图形管线vkPipeline--对应于subpass，需要为每个subpass创建对应的图形管线;
//图形管线（VkPipeline）在创建时，必须严格绑定到一个特定的 Render Pass 里的某一个特定的 Subpass 上;
/**
* 一个renderPass（工厂）可以包含多个subpass（工序，Subpass 0 负责画几何体，Subpass 1 负责光照计算，Subpass 2 负责 UI）,
* 一个VkPipeline（工人） 只能绑定在一个Subpass上,
* 一个Subpass可能对应多个vkPipeline：假设你在 Subpass 0（渲染几何体） 中，既要画一个不透明的木箱子，又要画一个半透明的玻璃窗。
* 木箱子不需要混合，玻璃窗需要颜色混合；木箱子用一套着色器，玻璃窗用另一套着色器。
* 这个时候，单单针对 Subpass 0，你就需要创建 2 个图形管线：
* 管线 A：关闭混合，绑定箱子的着色器，subpass = 0。
* 管线 B：开启混合，绑定玻璃的着色器，subpass = 0。
* 同一个subpass里面绘制的物体，只要使用不同的着色器，混合模式，深度测试/写入状态，光栅化状态，顶点数据结构大变等都需要新建管线，所以一个subpass可以对应多个vkpipeline
*/

/**
* 总结：
* 1个Render Pass包含N个Subpass;
* 1个Subpass可以包含N个vkPipeline（用于画不同的材质、物体）;
* 1个Pipeline只能严格属于1个特定的Subpass，它绝不能跨界;
*/
void HelloTriangleApplication::createGraphicsPipeline() {
    
    //1-配置着色器
    std::string exeDir = getExeDirectory();
    std::cout << "EXE 所在目录: " << exeDir << std::endl;
    auto vertShaderCode = readFile(exeDir + "/shaders/vert.spv");
    auto fragShaderCode = readFile(exeDir + "/shaders/frag.spv");

    //读取着色器字节码信息
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

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

    //2-配置固定管线功能
    //顶点输入--描述了将传递给顶点着色器的顶点数据的格式
    /*
    * 绑定：数据之间的间距以及数据是按顶点还是按实例（参见实例化）
      属性描述：传递给顶点着色器的属性类型、加载它们的绑定以及偏移量。
    */
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    //把包含顶点数据的内存块（VkBuffer）绑定到管线上，
    //一个 Binding 描述了整个数据块的总体特征：比如相邻两个顶点之间的字节间距（Stride 是多少），
    //以及数据是逐个顶点提取，还是逐个实例（Instance）提取。这里为 0 说明我们没有绑定任何外部缓冲区。
    vertexInputInfo.vertexBindingDescriptionCount = 0;//绑定的数据
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    // Attribute（属性）？：一个顶点通常包含多种信息，比如位置（x,y,z）、颜色（r,g,b）、纹理坐标（u,v）等。
    // 这些具体的细节就是属性。属性描述用来告诉 GPU：“位置信息在这个数据块的第 0 个字节，类型是 vec3；颜色信息在第 12 个字节，类型也是 vec3”。
    vertexInputInfo.vertexAttributeDescriptionCount = 0;//数据的属性
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional


    //图元类型
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    //视口与裁切--为什么没有定义具体的宽高？ 因为代码后面使用了动态状态（Dynamic State），具体的宽高可以在绘制命令录制时动态指定。
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    //光栅化相关
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    //多重采样-用于抗锯齿
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;//关闭抗锯齿
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    //颜色混合-针对渲染子通道中的帧缓冲附件，如果子通道有多个（N个）附件（MRT），那此处就需要创建多个（N个）对应的VkPipelineColorBlendAttachmentState，以便对每个输出附件进行混合设置
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    //glColorMask
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    //视口与裁切测试-允许动态状态修改，渲染过程中可修改
    /**
    * 前面说过，Vulkan 几乎所有的状态都要提前烘焙，如果你调整了窗口大小，视口（Viewport）改变了，难道要销毁并重新创建一个庞大的管线吗？
    * 为了解决这个问题，Vulkan 允许少数状态被标记为“动态的”，这里将视口和裁剪框设为动态，
    * 意味着可以在渲染时（Command Buffer中）调用 vkCmdSetViewport 随时改变它们的大小，而无需重建管线。
    */
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();


    //管道布局-全局变量使用-uniform
    /**
    * 管线布局用于告诉 GPU，着色器将会使用哪些全局变量（如 Uniform Buffers，通常用来传递 MVP 变换矩阵；或者 Push Constants，用于传递少量的高频更新数据）。
    * 目前是空布局（Count=0），因为最简单的三角形还不需要传递这些参数。但即使为空，Vulkan 也要求必须创建一个 VkPipelineLayout 对象。
    */
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(this->_logicDevice, &pipelineLayoutInfo, nullptr, &this->_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    //组装真正的渲染管线
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    //引用着色器功能
    pipelineInfo.pStages = shaderStages;
    //引用所有的固定功能
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    //引用管道布局
    pipelineInfo.layout = this->_pipelineLayout;
    //当前的vkPipeline引用渲染pass和子通道--图形管线必须知道它将在哪种渲染通道中工作（比如颜色附件的格式是什么，有没有深度缓冲等），管线和 RenderPass 是高度绑定的。
    pipelineInfo.renderPass = this->_renderPass;
    pipelineInfo.subpass = 0;

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
    //pipelineCache管道缓存比上面的管线继承更先进，管线缓存不仅能实现类似于派生的性能优化，还能把编译好的管线保存到硬盘上，下次玩家启动游戏时直接读取，实现真正的“秒开
    if (vkCreateGraphicsPipelines(this->_logicDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(this->_logicDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(this->_logicDevice, vertShaderModule, nullptr);
}

//关键步骤十：创建帧缓冲区
/**
* vulkan帧缓冲与OpenGL的帧缓冲稍微有些不同，两者的一致之处都是将渲染结果绘制到某张中间图像上或者屏幕上（vulkan是交换链），
* 不同的是vulkan的帧缓冲绑定renderPass，调用的时候需要传入renderPass和vkFrameBuffer，而OpenGL中只要调用了glBindFrameBuffer，后面所有的操作都受其影响。
* renderPass必须有对应的vkFrameBuffer，vulkan需要为每个renderPass创建vkFrameBuffer，vkFrameBuffer链接vkImageView和renderPass，这样renderPass就可以绘制到对应的vkImageView
*/
//创建帧缓冲，将renderPass和vkImageView连接起来
void HelloTriangleApplication::createFramebuffers()
{
    //调整容器大小，使其能够容纳所有的帧缓冲区
    this->_swapChainFramebuffers.resize(this->_swapChainImageViews.size());
    //遍历图像视图并从中创建帧缓冲区
    for (size_t i = 0; i < this->_swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {this->_swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = this->_renderPass;///frameBuffer绑定的renderPass
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;//vkFrameBuffer绑定到vkImageView,这儿是直接绑定到交换链，那当前renderPass会直接往屏幕上进行绘制
        framebufferInfo.width = this->_swapChainExtent.width;
        framebufferInfo.height = this->_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(this->_logicDevice, &framebufferInfo, nullptr, &this->_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

//关键步骤十一：创建命令池，用来管理命令缓冲区
/**
* 在 Vulkan中，不能直接像 OpenGL 那样发送指令给 GPU，需要先将指令（如绘制、绑定资源等）记录到命令缓冲区（Command Buffer）中，
* 然后将缓冲区提交给 GPU 队列,而命令池就是用来管理这些命令缓冲区内存的容器。
*/
void HelloTriangleApplication::createCommandPool()
{
    //QueueFamilyIndices queueFamilyIndices = findQueueFamilies(this->_physicalDevice);
    //队列族
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    /**
    * VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT允许从该池分配的命令缓冲区被单独重置（Reset）。
    * 在渲染循环中，我们通常每一帧都要重新记录命令。如果不设置这个标志，你必须重置整个命令池（即重置池中所有的缓冲区）才能重新使用其中的内存；
    * 设置了它之后，你可以调用 vkResetCommandBuffer 来单独覆盖某个缓冲区的指令，灵活性更高。
    */
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;//允许单独重新记录命令缓冲区，如果没有此标志，则所有缓冲区必须一起重置
    /**
    * 命令缓冲区通过提交到设备队列来执行，不同的队列执行的任务不同（有的处理图形，有的处理计算，有的处理内存传输）。
    * 限制：一个命令池只能为特定的一种队列族创建命令缓冲区。
    * 这里使用了 _graphicsFamily（图形队列族），说明从这个池里创建出来的命令，将来是发给 GPU 用来画图的。
    */
    poolInfo.queueFamilyIndex = this->_physicalQueueFamilyIndices._graphicsFamily.value();
    //创建命令池
    if (vkCreateCommandPool(this->_logicDevice, &poolInfo, nullptr, &this->_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

//关键步骤十二：分配命令缓冲区--从之前创建的命令池中分配单个具体的命令缓冲区
//在 Vulkan 中，命令池就像是一大张还没写字的纸，而命令缓冲区则是从这张纸上裁下来的“活页”，让你可以在上面记录具体的 GPU 指令（如画一个三角形、切换贴图等）。
void HelloTriangleApplication::createCommandBuffer() {
    //1-分配而非创建：
    //Create 通常意味着从系统申请内存，开销较大。
    //Allocate 意味着从已经存在的资源池（Command Pool）中“拨”出一块内存。这样非常高效，因为命令池已经预先管理好了内存。
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = this->_commandPool;
    /**
    *  VK_COMMAND_BUFFER_LEVEL_PRIMARY:主命令缓冲区--它可以被直接提交到 GPU 队列（Queue）执行，它可以执行（调用）二级命令缓冲区。
    *  VK_COMMAND_BUFFER_LEVEL_SECONDARY:二级命令缓冲区--不能直接提交给GPU，但可以被主命令缓冲区调用（类似于函数调用）。
    */
    //设置为分配主命令缓冲区
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;//指定一次性分配多少个缓冲区，这里设置为1
    //执行分配--分配完成后_commandBuffer句柄就可用了，但注意：此时的命令缓冲区是空的，而且处于“初始状态（Initial state）”。你还不能把它交给 GPU。
    if (vkAllocateCommandBuffers(this->_logicDevice, &allocInfo, &this->_commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

//关键步骤十三：录制命令缓冲区--分配好命令缓冲区之后，就要开始将要执行的命令写入到命令​​缓冲区
//在 Vulkan 中，不能直接下令“画一个三角形”。必须把所有的指令（开启渲染通道、绑定管线、设置视口、执行绘制等）按顺序“录制”到一个缓冲区里，最后一次性提交给 GPU 执行。
void HelloTriangleApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
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
    renderPassInfo.renderPass = this->_renderPass;
    //为每个交换链图像创建了一个帧缓冲区,并将其指定为颜色附件
    //绑定的帧缓冲区
    renderPassInfo.framebuffer = this->_swapChainFramebuffers[imageIndex];

    //渲染区域大小
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = this->_swapChainExtent;

    //清屏颜色
    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    /**
    *VK_SUBPASS_CONTENTS_INLINE渲染通道命令将嵌入到主命令缓冲区本身中，不会执行任何辅助命令缓冲区。
    *VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS渲染通道命令将从辅助命令缓冲区执行。
    */
    //开始渲染-记录，这告诉 GPU 渲染正式开始
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    //-绑定图形管线：图形管线（Graphics Pipeline）包含了着色器（Shader）、混合模式、深度测试等所有状态。绑定了它，接下来的绘制就会按照你预设的一套规则进行。
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->_graphicsPipeline);
    
    //如果你在创建管线时设置了视口（Viewport）和裁剪（Scissor）为 Dynamic State，那么你必须在录制时手动设置它们。
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(this->_swapChainExtent.width);
    viewport.height = static_cast<float>(this->_swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = this->_swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    //发出绘制指令，真正执行“画”的动作
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    //渲染过程结束
    vkCmdEndRenderPass(commandBuffer);
    //命令缓冲区录制完毕--将缓冲区从“录制状态”转变为“可提交状态（Executable state）”。
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

//关键步骤十四：创建同步对象-信号量/栅栏，信号量用于GPU同步，栅栏用于CPU同步
void HelloTriangleApplication::createSyncObjects() {
    //信号量
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    //栅栏
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    //创建栅栏时带有信号，这样就可以防止绘制函数drawFrame()第一次无限等待
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(this->_logicDevice, &semaphoreInfo, nullptr, &this->_imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(this->_logicDevice, &semaphoreInfo, nullptr, &this->_renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(this->_logicDevice, &fenceInfo, nullptr, &this->_inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create semaphores!");
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
void HelloTriangleApplication::drawFrame() {
    /**
    * 1-每一帧开始时等待上一帧
    * VK_TRUE--表示等待所有的栅栏
    * timeout--表示等待的超时参数
    * UINT64_MAX参数表示如果栅栏一直没有信号，我们将一直等待
    */
    //----------------------------------阻塞------------------------------------------------
    //此函数将阻塞cpu，一直等待gpu发送上一帧绘制完成为止
    vkWaitForFences(this->_logicDevice, 1, &this->_inFlightFence, VK_TRUE, UINT64_MAX);
    //信号量发送成功，等待完成，重置栅栏为无信号状态
    vkResetFences(this->_logicDevice, 1, &this->_inFlightFence);
    /**
    * 2-从交换链获取图像--会一直阻塞，获取不到图像会一直阻塞，直到timeout，如果timeout = UINT64_MAX将会一直等待，程序会卡住在此处，
    * 获取成功后，函数阻塞结束，imageIndex将可用，但是this->_imageAvailableSemaphor却不一定有信号，因为屏幕端可能仍在扫描这张图片的最后几行，
    * 但是函数阻塞结束后，程序将继续往下运行，可以进行顶点变换，光栅化等操作，直到运行到需要等待this->_imageAvailableSemaphor的地方，
    * 当展示引擎完成所有的工作，彻底释放显存，gpu会触发this->_imageAvailableSemaphor这个信号量，然后渲染引擎将往这张图像进行写入操作。
    */
    uint32_t imageIndex;
    //----------------------------------阻塞------------------------------------------------
    vkAcquireNextImageKHR(this->_logicDevice, this->_swapChain, UINT64_MAX, this->_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    //3-重置命令缓冲区-以便其能够被录制
    vkResetCommandBuffer(this->_commandBuffer, 0);
    //4-录制命令缓冲区
    recordCommandBuffer(this->_commandBuffer, imageIndex);
    
    //5-提交命令缓冲区，录制结束，commandBuffer提交到gpu准备执行
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    /**指定等待信号量-信号量有信号才开始执行，
    * 指定等待的阶段，在最终要写入图像时才开始等待，
    * 而顶点输入 -> 顶点着色器 -> 几何着色器 -> 光栅化 -> 片元着色器这些阶段都可以执行不需要等待，
    * 当最终要写入帧缓冲时，必须等待可用的图像进行写入，
    * 在图像可用时才向图像写入颜色，因此我们指定了图形管线中写入颜色附件的阶段
    */
    VkSemaphore waitSemaphores[] = { this->_imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    //指定要提交执行的命令缓冲区
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &this->_commandBuffer;

    //指定命令缓冲区执行完毕后要向_renderFinishedSemaphore信号量发出信号
    VkSemaphore signalSemaphores[] = { this->_renderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    /**
    * 命令缓冲区提交到图形绘制队列/绘制引擎（作用于后缓冲），开始执行，
    * 最后一个参数指向一个可选的“栅栏”，当命令缓冲区执行完毕时，该栅栏会发出信号，在下一帧中，CPU 将等待此命令缓冲区执行完毕，同时触发_renderFinishedSemaphore信号量发出信号，
    * 即命令缓冲区执行完毕时同时触发_inFlightFence和_renderFinishedSemaphore两个信号量。
    */
    if (vkQueueSubmit(this->_graphicsQueue, 1, &submitInfo, this->_inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    //绘制完成后将图像提交给交换链，准备呈现，最终使其显示在屏幕上
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { this->_swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pResults = nullptr; // Optional
    //提交到呈现队列/展示引擎-（作用于前缓冲)-准备显示--vkQueuePresentKHR函数会向交换链提交显示图像的请求
    vkQueuePresentKHR(this->_presentQueue, &presentInfo);
}
