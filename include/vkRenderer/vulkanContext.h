#pragma once

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
    std::optional<uint32_t> _graphicsAndComputeFamily;//支持图形绘制指令的队列族
    std::optional<uint32_t> _presentFamily;//支持呈现的队列族,确保设备可以在我们创建的表面上显示图像
    bool isComplete()
    {
        return this->_graphicsAndComputeFamily.has_value() && this->_presentFamily.has_value();
        //return this->_graphicsAndComputeFamily >= 0 && this->_presentFamily >= 0;
    }
};


class VulkanContext {
public:
    // 构造函数中执行整个基础初始化流程
    VulkanContext(uint32_t width, uint32_t height, const std::string& title);

    // 禁止拷贝 (Vulkan 资源通常是独占的)
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // ----- Getters (供上层使用) -----
    GLFWwindow* getWindow() const { return _glfwWindow; }
    VkInstance getInstance() const { return _vkInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return _physicalDevice; }
    VkDevice getLogicDevice() const { return _logicDevice; }
    VkSurfaceKHR getSurface() const { return _vkSurface; }
    VkQueue getGraphicsQueue() const { return _graphicsQueue; }
    VkQueue getPresentQueue() const { return _presentQueue; }
    VkQueue getComputeQueue() const { return _computeQueue; }

    //队列族
    void findQueueFamilies(VkPhysicalDevice device);

    QueueFamilyIndices getPhysicalQueueFamilyIndices()const
    {
        return this->_physicalQueueFamilyIndices;
    }

    VkSampleCountFlagBits getMsaaSamples() { return this->_msaaSamples; }

    void cleanup();

    bool _framebufferResized = false;

private:
    GLFWwindow* _glfwWindow = nullptr;
    VkInstance _vkInstance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR _vkSurface = VK_NULL_HANDLE;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    VkDevice _logicDevice = VK_NULL_HANDLE;

    //队列族
    QueueFamilyIndices _physicalQueueFamilyIndices;

    VkQueue _graphicsQueue = VK_NULL_HANDLE;
    VkQueue _presentQueue = VK_NULL_HANDLE;
    VkQueue _computeQueue = VK_NULL_HANDLE;

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    const std::vector<const char*> _validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    //物理设备扩展，交换链扩展
    const std::vector<const char*> _physicalDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    /**
    * VkApplicationInfo（optional）应用程序信息 - 这些信息的填写不是必须的，但填写的信息可能会作为驱动程序的优化依据，让驱动程序进行一些特殊的优化。
    * 比如：应用程序使用了某个引擎，驱动程序对这个引擎有一些特殊处理，这时就可能有很大的优化提升。
    */
    //static VkApplicationInfo appInfo;
    //static VkInstanceCreateInfo vkInstanceCreateInfo;//（require）vulkan驱动程序需要使用的全局扩展和校验层


    // ----- 私有初始化步骤 -----
    void initWindow(uint32_t width, uint32_t height, const std::string& title);
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicDevice();

    //获取最大采样样本
    VkSampleCountFlagBits getMaxUsableSampleCount();

    // 辅助检查函数
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    //void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    //创建VkDebugUtilsMessengerEXT//存储回调函数信息-启用校验层以后需要设置回调函数来获得回调信息
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

    //得到物理显卡的扩展属性列表，并判断需要的扩展-主要判断交换链扩展是否在列表里面
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
};