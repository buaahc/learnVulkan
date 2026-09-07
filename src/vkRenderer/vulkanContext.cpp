#include<vector>
#include<map>
#include<set>


#define VK_USE_PLATFORM_WIN32_KHR
#include"vulkanContext.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <chrono>
#include <unordered_map>
#include <random>
#include <ctime>
//#include "tools.h"


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


//窗口大小回调
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    VulkanContext* app = reinterpret_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
    app->_framebufferResized = true;
}

VulkanContext::VulkanContext(uint32_t width, uint32_t height, const std::string& title)
{
    this->initWindow(width, height, title);
}

void VulkanContext::initWindow(uint32_t width, uint32_t height, const std::string& title) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//不要创建OpenGL上下文
    this->_glfwWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(this->_glfwWindow, this);//HelloTriangleApplication实例绑定到_glfwWindow中，类似于userData
    glfwSetFramebufferSizeCallback(this->_glfwWindow, framebufferResizeCallback);//设置窗口大小发生变化时的回调函数
    //this->_lastTime = glfwGetTime();
}

void VulkanContext::createInstance()
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

    VkApplicationInfo appInfo = VkApplicationInfo();
    VkInstanceCreateInfo vkInstanceCreateInfo = VkInstanceCreateInfo();

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;//指定使用vulkan1.0版本

    vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstanceCreateInfo.pApplicationInfo = &appInfo;

    //为了能在屏幕上显示窗口并且方便找Bug，需要开启哪些扩展（Extensions）”
    //vulkan是平台无关的API，所以需要一个和窗口系统交互的扩展，glfw库包含了一个可以返回这一扩展的函数，可以直接使用它
    //VK_EXT_DEBUG_UTILS扩展，为了与窗口进行调试回调
    std::vector<const char*> glfwExtensions = getRequiredExtensions();//得到glfw窗口扩展+VK_EXT_DEBUG_UTILS调试扩展
    /**
    * 注意：我们得到程序所需的glfw窗口扩展+VK_EXT_DEBUG_UTILS调试扩展后，应该与上面vulkanAllExtensions，进行比对，看是否都支持
    */
    //挂载扩展层
    vkInstanceCreateInfo.enabledExtensionCount = glfwExtensions.size();
    vkInstanceCreateInfo.ppEnabledExtensionNames = glfwExtensions.data();

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
        vkInstanceCreateInfo.enabledLayerCount = this->_validationLayers.size();
        vkInstanceCreateInfo.ppEnabledLayerNames = this->_validationLayers.data();
        //调试信使--填充调试信使的配置信息，告诉 Vulkan 你希望接收什么级别的调试信息、什么类型的调试信息，以及接收到信息后该交给哪个函数处理。
        populateDebugMessengerCreateInfo(debugCreateInfo);
        vkInstanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        vkInstanceCreateInfo.enabledLayerCount = 0;
        vkInstanceCreateInfo.pNext = nullptr;
    }
    if (vkCreateInstance(&vkInstanceCreateInfo, nullptr, &this->_vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

//Vulkan 本身是一个平台无关的 API，这意味着它只负责图形渲染，不知道如何与特定操作系统（如 Windows, macOS, Linux）的窗口系统进行交互。
// 要在窗口中显示画面，我们需要启用特定的窗口扩展（Extensions），glfw扩展。

/**实例级别的扩展，创建instance时使用，
 * 作用域： 它是针对整个 Vulkan 运行环境的，与你电脑里插了哪张显卡毫无关系。
 * 它干了什么：它主要返回为了让 Vulkan 能和操作系统（Windows / Linux）打交道所需的扩展。
*/
std::vector<const char*> VulkanContext::getRequiredExtensions() {
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
bool VulkanContext::checkValidationLayerSupport() {
    uint32_t layerCount;
    //负责统计并返回当前操作系统中注册过的所有 Vulkan 层的信息
    //请求这台电脑上当前都安装了哪些 Vulkan 全局层？
    //得到所有的全局图层，并检查校验层是否可用
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    availableLayers.resize(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : this->_validationLayers) {
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
    //createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
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
VkResult VulkanContext::CreateDebugUtilsMessengerEXT(
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
void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

//2.5-上层调用创建调试信使--根据回调函数信息VkDebugUtilsMessengerCreateInfoEXT createInfo，创建VkDebugUtilsMessengerEXT实例_debugMessenger
void VulkanContext::setupDebugMessenger() {
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
void VulkanContext::createSurface()
{
    //原先注释
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
void VulkanContext::findQueueFamilies(VkPhysicalDevice physicalDevice) {
    // Logic to find queue family indices to populate struct with
    //首先获取设备的队列族个数
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        //1-查找图形绘制队列族-其实就是一个uint32_t类型索引index--找到支持VK_QUEUE_GRAPHICS_BIT/VK_QUEUE_COMPUTE_BIT绘制指令的队列族
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)&&
            (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))//同时支持图形和计算的队列族
        {
            this->_physicalQueueFamilyIndices._graphicsAndComputeFamily = i;
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
}


//4.2-得到物理显卡的扩展属性列表，并判断需要的扩展-主要判断交换链扩展是否在列表里面
/**
* 设备级别扩展：
* 什么时候调用？ 在挑选显卡（VkPhysicalDevice）以及创建逻辑设备（VkDevice，即调用 vkCreateDevice）时调用。
* 作用域： 它是严格针对某一张具体的显卡的。电脑里的集显可能不支持某个扩展，但独显可能支持。
* 它干了什么？
* 在您的代码中，它检查了这块特定的显卡（device 参数）是否支持 VK_KHR_swapchain（交换链扩展）。
* 因为“把渲染好的图像送到屏幕的显示缓冲里”（即交换链的功能）是极其依赖底层显卡硬件特性的。某些计算专用显卡（如用于 AI 训练的 Tesla 卡）就没有视频输出接口，因此它们就不支持这个设备扩展。
* 除了交换链，如果以后您想用“光线追踪”，您也是在这里检查显卡支不支持 VK_KHR_ray_tracing_pipeline 这个设备扩展。
* 
*/
//判断是否支持交换链扩展
bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) {

    //得到显卡支持的所有扩展列表
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(this->_physicalDeviceExtensions.begin(), this->_physicalDeviceExtensions.end());

    //判断需要的扩展-交换链是否在显卡所支持的扩展列表里面，检查交换链是否可用
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

//4.4-选择合适的显卡--绘制和呈现的队列族，支持将图像输出到创建的窗口表面_vkSurface，支持交换链扩展，查询显卡的相关能力需支持交换链的需求
bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) {

    //1-判断物理设备支持的队列族（绘制队列族，呈现队列族，支持将图像输出到我们创建的窗口表面）
    this->findQueueFamilies(device);
    //2-物理设备是否支持特定的扩展，目前是交换链扩展
    bool swapchainExtensionsSupported = checkDeviceExtensionSupport(device);
    return this->_physicalQueueFamilyIndices.isComplete() && swapchainExtensionsSupported;

#if 0
    //3-查询交换链支持的详细信息-交换链图像大小/数量/图像格式/图像呈现方式
    bool swapChainAdequate = false;
    if (swapchainExtensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return this->_physicalQueueFamilyIndices.isComplete() && swapchainExtensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;//强制启用各异向性过滤
#endif // 0
}

//4.5-选择最终的物理显卡
void VulkanContext::pickPhysicalDevice() {
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
            this->_msaaSamples = this->getMaxUsableSampleCount();
            break;
        }
    }
    if (this->_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void VulkanContext::createLogicDevice()
{
    //1-指定要创建的逻辑队列（绘制/呈现），可以理解为传送带，物理队列族（绘制/呈现）关联到逻辑设备信息，std::set自动去重，如果_graphicsAndComputeFamily和_presentFamily相同，std::set将保证只使用一个；
    std::set<uint32_t> uniqueQueueFamilies = { this->_physicalQueueFamilyIndices._graphicsAndComputeFamily.value(), this->_physicalQueueFamilyIndices._presentFamily.value() };
    float queuePriority = 1.0f;
    //指定创建的队列
    std::vector<VkDeviceQueueCreateInfo> logicDeviceQueueCreateInfos;

    for (uint32_t physicalQueueFamily : uniqueQueueFamilies) {
        //为每个索引创建一个 VkDeviceQueueCreateInfo 并存入 _logicDeviceQueueCreateInfos 向量中
        VkDeviceQueueCreateInfo logicDeviceQueueCreateInfo{};
        logicDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        logicDeviceQueueCreateInfo.queueFamilyIndex = physicalQueueFamily;
        logicDeviceQueueCreateInfo.queueCount = 1;
        //你必须给传送带定个优先级（0.0 到 1.0），这就好比告诉工厂：“这条传送带上的任务最重要（1.0），给我优先处理”。
        logicDeviceQueueCreateInfo.pQueuePriorities = &queuePriority;
        logicDeviceQueueCreateInfos.push_back(logicDeviceQueueCreateInfo);//std::vector<VkDeviceQueueCreateInfo> _logicDeviceQueueCreateInfos;
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    //deviceFeatures.sampleRateShading = VK_TRUE;

    VkDeviceCreateInfo vkLogicDeviceCreateInfo;

    vkLogicDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkLogicDeviceCreateInfo.pQueueCreateInfos = logicDeviceQueueCreateInfos.data();//指向队列创建信息
    vkLogicDeviceCreateInfo.queueCreateInfoCount = logicDeviceQueueCreateInfos.size();

    //2-指定使用的设备特性
    //告诉Vulkan打算使用物理设备的哪些高级特性（例如几何着色器、多点采样、各向异性过滤等）,通常这些特性在之前的“挑选物理设备”阶段已经查询过了。
    //this->_vkLogicDeviceCreateInfo.pEnabledFeatures = &this->_physicalDeviceFeatures;
    vkLogicDeviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    //3-启用交换链扩展--逻辑设备支持交换链扩展
    //逻辑设备本身不包含显示画面的功能，为了让显卡能把图像显示到 Windows/Linux 窗口上，必须启用特定的扩展，最常见的是 VK_KHR_swapchain（交换链扩展）。
    vkLogicDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(this->_physicalDeviceExtensions.size());
    vkLogicDeviceCreateInfo.ppEnabledExtensionNames = this->_physicalDeviceExtensions.data();

    //4-使用与vkInstance相同的校验层，创建逻辑设备添加校验层信息
    if (enableValidationLayers) {
        vkLogicDeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(this->_validationLayers.size());
        vkLogicDeviceCreateInfo.ppEnabledLayerNames = this->_validationLayers.data();
    }
    else {
        vkLogicDeviceCreateInfo.enabledLayerCount = 0;
    }
    //5-创建逻辑设备
    if (vkCreateDevice(this->_physicalDevice, &vkLogicDeviceCreateInfo, nullptr, &this->_logicDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    //6-从逻辑设备上得到图形队列族的句柄，拿到向显卡发送命令的通道，有了逻辑设备和队列句柄，我们现在就可以真正开始使用显卡来执行任务了
    vkGetDeviceQueue(this->_logicDevice, this->_physicalQueueFamilyIndices._graphicsAndComputeFamily.value(), 0, &this->_graphicsQueue);
    vkGetDeviceQueue(this->_logicDevice, this->_physicalQueueFamilyIndices._graphicsAndComputeFamily.value(), 0, &this->_computeQueue);
    vkGetDeviceQueue(this->_logicDevice, this->_physicalQueueFamilyIndices._presentFamily.value(), 0, &this->_presentQueue);
}

//获取最大采样样本
VkSampleCountFlagBits VulkanContext::getMaxUsableSampleCount() {
    //查询设备的基本属性，例如名称、类型和支持的 Vulkan 版本
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(this->_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanContext::cleanup()
{
    //销毁逻辑设备
    vkDestroyDevice(this->_logicDevice, nullptr);
    //销毁调试层
    if (enableValidationLayers)
        DestroyDebugUtilsMessengerEXT(this->_vkInstance, this->_debugMessenger, nullptr);
    //销毁窗口表面
    vkDestroySurfaceKHR(this->_vkInstance, this->_vkSurface, nullptr);
    vkDestroyInstance(this->_vkInstance, nullptr);
    glfwDestroyWindow(this->_glfwWindow);

}
