#pragma once
#include <vulkan/vulkan.h>
#include <vector>
class VulkanContext;
class VulkanCommandManager;


// （从原来的 compute.h 搬运过来）查询交换链支持细节的结构体
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanSwapChain {
public:
    // 构造时需要 Context 提供设备句柄，同时也需要知道窗口当前的实际大小
    VulkanSwapChain(std::shared_ptr<VulkanContext> context, std::shared_ptr<VulkanCommandManager>vkCommandManager);
    ~VulkanSwapChain();

    // 禁用拷贝
    VulkanSwapChain(const VulkanSwapChain&) = delete;
    void operator=(const VulkanSwapChain&) = delete;

    // ----- 获取渲染目标的核心资源 -----
    VkSwapchainKHR getSwapChain() const { return _swapChain; }
    VkFormat getSwapChainImageFormat() const { return _swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return _swapChainExtent; }
    VkRenderPass getRenderPass() const { return _renderPass; }

    // 获取指定帧的 Framebuffer，用于 recordCommandBuffer
    VkFramebuffer getFramebuffer(uint32_t index) const { return _swapChainFramebuffers[index]; }

    size_t getImageCount() const { return _swapChainImages.size(); }

    // 窗口调整大小时需要调用
    void recreateSwapChain();

    void createSwapChain();


    void createRenderPass(); // 您刚刚完美修改过的那个包含 3 个附件的函数
    void createFramebuffers();

    void createDepthResources();
    void createMultiSamplerColorResources(); // 如果开启 MSAA

    void cleanup();
private:
    std::shared_ptr<VulkanContext> _vkContext = nullptr; // 持有 Context 引用
    std::shared_ptr<VulkanCommandManager> _vkCommandManager = nullptr; // 持有 commandManager 引用

    VkSwapchainKHR _swapChain;
    VkFormat _swapChainImageFormat;
    VkExtent2D _swapChainExtent;

    std::vector<VkImage> _swapChainImages;
    std::vector<VkImageView> _swapChainImageViews;

    // ----- 附件资源 (颜色, MSAA, 深度) -----
    VkImage _depthImage;
    VkDeviceMemory _depthImageMemory;
    VkImageView _depthImageView;
    
    // 如果有 MSAA 相关变量也放这里，多重采样缓冲区
    VkImage _multiSampleColorImage;
    VkDeviceMemory _multiSampleColorImageMemory;
    VkImageView _multiSampleColorImageView;


    VkRenderPass _renderPass;
    std::vector<VkFramebuffer> _swapChainFramebuffers;


    // 辅助查询函数
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
};