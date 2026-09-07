
#include "vulkanContext.h"
#include "vulkanSwapChain.h"
#include "vulkanCommandManager.h"
#include <algorithm> // Necessary for std::clamp
#include "readBuffer.h"

VulkanSwapChain::VulkanSwapChain(
    std::shared_ptr<VulkanContext> context, 
    std::shared_ptr<VulkanCommandManager>vkCommandManager)
{
    this->_vkContext = context;
    this->_vkCommandManager = vkCommandManager;
}


VulkanSwapChain::~VulkanSwapChain()
{
    this->cleanup();
    vkDestroyRenderPass(this->_vkContext->getLogicDevice(), this->_renderPass, nullptr);
}

//4.3-查询尺寸能力（Capabilities）、色彩格式（Formats）、显示模式（Present Modes）
// 在准备创建“交换链（Swapchain）”之前，全面调查一下显卡（物理设备）和窗口（Surface）配合工作时，到底具备哪些能力和限制，
// 需要知道能画多大尺寸的画、用什么颜料、画画的速度有多快，呈现的模式是啥，
SwapChainSupportDetails VulkanSwapChain::querySwapChainSupport(VkPhysicalDevice physicalDevice)
{
    SwapChainSupportDetails details;
    //1-查询基本能力，查询显卡在当前窗口表面上绘图的基础物理属性限制，
    // 交换链图像数量（通常2张，双缓冲）/分辨率限制/变换支持：当前是否支持旋转
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, this->_vkContext->getSurface(), &details.capabilities);

    //2-查询显卡和窗口组合支持哪些像素颜色格式和色彩空间- VK_FORMAT_B8G8R8A8_SRGB
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->_vkContext->getSurface(), &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->_vkContext->getSurface(), &formatCount, details.formats.data());
    }

    //3-查询支持的显示模式，呈现模式
    //VK_PRESENT_MODE_IMMEDIATE_KHR：画完立刻显示。会导致画面撕裂（Tearing）。
    //VK_PRESENT_MODE_FIFO_KHR：相当于垂直同步（V - Sync）。画好的图排队等屏幕刷新，这是 Vulkan 唯一保证所有显卡都支持的模式。
    //VK_PRESENT_MODE_MAILBOX_KHR：三重缓冲。如果画得太快，新的图会直接替换掉队列里旧的图，既能保证低延迟，又不会画面撕裂。（通常是游戏的首选）。
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->_vkContext->getSurface(), &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->_vkContext->getSurface(), &presentModeCount, details.presentModes.data());
    }

    return details;
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
VkSurfaceFormatKHR VulkanSwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
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
VkPresentModeKHR VulkanSwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) //三缓冲模式
        {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;//双缓冲-垂直同步
}

//6.3-选择交换范围--交换链中图像的分辨率
VkExtent2D VulkanSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT_MAX/*std::numeric_limits<uint32_t>::max()*/)
    {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(this->_vkContext->getWindow(), &width, &height);

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
void VulkanSwapChain::createSwapChain() {
    //1-查询尺寸能力（Capabilities）、色彩格式（Formats）、显示模式（Present Modes）
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(this->_vkContext->getPhysicalDevice());

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
    swapchainCreateInfo.surface = this->_vkContext->getSurface();

    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;//每个图像包含的层次，除非在做 VR（双眼立体渲染需要2层）或者立方体渲染，否则通常都是1。
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//图像用途，把这些图像当作颜色附件，也就是直接往上面画图（渲染输出）


    uint32_t queueFamilyIndices[] = { this->_vkContext->getPhysicalQueueFamilyIndices()._graphicsAndComputeFamily.value(), this->_vkContext->getPhysicalQueueFamilyIndices()._presentFamily.value() };

    //存在多个队列族时，如何处理交换链图形，队列族中的图形队列族与呈现队列族不同，将从图形队列绘制交换链中的图像，然后将其提交到表示队列
    if (this->_vkContext->getPhysicalQueueFamilyIndices()._graphicsAndComputeFamily != this->_vkContext->getPhysicalQueueFamilyIndices()._presentFamily) {
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
    if (vkCreateSwapchainKHR(this->_vkContext->getLogicDevice(), &swapchainCreateInfo, nullptr, &this->_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    //得到交换链真实的图像句柄-_swapChainImages-VkImage
    vkGetSwapchainImagesKHR(this->_vkContext->getLogicDevice(), this->_swapChain, &imageCount, nullptr);
    //VkImage
    this->_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(this->_vkContext->getLogicDevice(), this->_swapChain, &imageCount, this->_swapChainImages.data());


    this->_swapChainExtent = extent;
    this->_swapChainImageFormat = surfaceFormat.format;

    //创建交换链图像视图，为每个VkImage创建imageView
    /**可以理解为vkImage创建使用说明书，为交换链中的每一张“原始画板”配上一双“眼睛”和一套“说明书”。
     * vkImage可以理解是一块资源，他可以仅包含一张图像（texture2D），也可以包含6张图像（cubeTexture），或者textureArray（可以包含N层图像）,
     * vkImageView可以理解为视图，如何使用这块资源
    */
    this->_swapChainImageViews.resize(this->_swapChainImages.size());
    for (size_t i = 0; i < this->_swapChainImages.size(); i++)
    {
        this->_swapChainImageViews[i] = vkDB::createImageView(
            this->_swapChainImages[i],
            _swapChainImageFormat, 
            VK_IMAGE_ASPECT_COLOR_BIT, 1,
            this->_vkContext->getLogicDevice());
    }
}

//6.5-重建交换链--比如窗口大小发生变化，窗口大小发生变化后surface大小也会相应的发生变化，但是交换链大小不会变，两者尺寸大小不一致，呈现肯定出现错误，所以必须要重建交换链--
//重建之前要先进行销毁
void VulkanSwapChain::cleanup() {

    vkDestroyImageView(this->_vkContext->getLogicDevice(), this->_multiSampleColorImageView, nullptr);
    vkDestroyImage(this->_vkContext->getLogicDevice(), this->_multiSampleColorImage, nullptr);
    vkFreeMemory(this->_vkContext->getLogicDevice(), this->_multiSampleColorImageMemory, nullptr);

    vkDestroyImageView(this->_vkContext->getLogicDevice(), this->_depthImageView, nullptr);
    vkDestroyImage(this->_vkContext->getLogicDevice(), this->_depthImage, nullptr);
    vkFreeMemory(this->_vkContext->getLogicDevice(), this->_depthImageMemory, nullptr);

    for (auto framebuffer : this->_swapChainFramebuffers) {
        vkDestroyFramebuffer(this->_vkContext->getLogicDevice(), framebuffer, nullptr);
    }

    for (auto imageView : this->_swapChainImageViews) {
        vkDestroyImageView(this->_vkContext->getLogicDevice(), imageView, nullptr);
    }

    vkDestroySwapchainKHR(this->_vkContext->getLogicDevice(), this->_swapChain, nullptr);
}

//6.6-重建交换链
/**
* 重建时机：
* vkAcquireNextImageKHR和vkQueuePresentKHR通过返回特殊值来指示交换链已经不适应，需要重建；
* VK_ERROR_OUT_OF_DATE_KHR交换链与表面不兼容，无法再用于渲染。这种情况通常发生在窗口大小调整之后。
* VK_SUBOPTIMAL_KHR交换链仍然可以用于成功地呈现到表面，但表面属性不再完全匹配。
*/
void VulkanSwapChain::recreateSwapChain()
{
    //如果是窗口最小化，将暂停渲染
    int width = 0, height = 0;
    glfwGetFramebufferSize(this->_vkContext->getWindow(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(this->_vkContext->getWindow(), &width, &height);
        //等待下一个事件（比如窗口还原）
        glfwWaitEvents();
    }
    //CPU强制阻塞操作--让 CPU 停下来，等待 GPU 把手里所有的活儿全部干完。
    //只有当 GPU 队列里所有的命令都执行完了，GPU 彻底进入“闲置（Idle）”状态时，这个函数才会返回，CPU 才能继续执行
    vkDeviceWaitIdle(this->_vkContext->getLogicDevice());
    //重建之前需要先进行销毁
    this->cleanup();
    this->createSwapChain();
    this->createMultiSamplerColorResources();
    this->createDepthResources();
    this->createFramebuffers();
}

//拿着一个“期望的格式列表”（优先级从高到低），挨个用 vkGetPhysicalDeviceFormatProperties 去问显卡。
//只要发现显卡返回的 props 里，满足了要求的 tiling（内存布局）和 features（功能，比如必须能做深度附件），它就立刻返回这个格式，作为最终的选择。
//检测 VkFormat 是否支持程序要求的 tiling（内存布局）和 features（功能，比如必须能做深度附件）
VkFormat VulkanSwapChain::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat imageFormat : candidates) {
        /**VkFormatProperties:
        * linearTilingFeatures线性平铺支持的用例
        * optimalTilingFeatures：支持最佳平铺效果的使用场景。
        * bufferFeatures缓冲区支持的用例
        */
        //结构体包含了显卡对某一种图像格式（imageFormat）支持的所有功能集。
        
        //vkGetPhysicalDeviceFormatProperties--询问显卡（_physicalDevice）：“对于 imageFormat 这个格式，都能对它做哪些操作
        VkFormatProperties props;//props 结构体返回 imageFormat 格式支持的所有功能集
        vkGetPhysicalDeviceFormatProperties(this->_vkContext->getPhysicalDevice(), imageFormat, &props);

        //开始检测内存布局和功能集是否符合要求
        
        //VK_IMAGE_TILING_LINEAR：线性排列，也就是像素一行接一行，CPU 读写很方便，但 GPU 访问慢
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return imageFormat;
        }
        //VK_IMAGE_TILING_OPTIMAL：最优块状排列的，显卡专属黑盒格式，GPU 访问极快，但 CPU 读不懂
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return imageFormat;
        }

    }
    throw std::runtime_error("failed to find supported format!");
}

/**
* VK_FORMAT_D32_SFLOAT：32 位浮点数表示深度
* VK_FORMAT_D32_SFLOAT_S8_UINT：深度采用 32 位有符号浮点数，模板分量采用 8 位浮点数。
* VK_FORMAT_D24_UNORM_S8_UINT：深度采用 24 位浮点数，模板分量采用 8 位浮点数。
*/
VkFormat VulkanSwapChain::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

//深度缓存-查找格式/创建图像
//VK_IMAGE_TILING_LINEAR（线性布局）, CPU 可以直接理解这种格式, GPU 访问效率非常低;
//VK_IMAGE_TILING_OPTIMAL（最优布局 / 瓦片布局）, 像素在内存中是以显卡厂商私有的、优化过的块状（Block / Tile）方式存放的, GPU 访问效率极高！
void VulkanSwapChain::createDepthResources()
{
    VkFormat depthFormat = this->findDepthFormat();
    vkDB::createImage(
        this->_swapChainExtent.width,
        this->_swapChainExtent.height, 1,
        this->_vkContext->getMsaaSamples(),
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT，显卡专用内存，GPU读取非常快
        this->_depthImage, this->_depthImageMemory,
        this->_vkContext->getLogicDevice(),
        this->_vkContext->getPhysicalDevice()
    );
    this->_depthImageView = vkDB::createImageView(this->_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, this->_vkContext->getLogicDevice());

    //布局转换
    VkCommandBuffer commandBuffer = this->_vkCommandManager->beginSingleTimeCommands();
    vkDB::transitionImageLayout(this->_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, commandBuffer);
    this->_vkCommandManager->endSingleTimeCommands(commandBuffer);
}

//多重采样颜色缓冲区
void VulkanSwapChain::createMultiSamplerColorResources()
{
    VkFormat colorFormat = this->_swapChainImageFormat;
    vkDB::createImage(
        this->_swapChainExtent.width,
        this->_swapChainExtent.height,
        1,
        this->_vkContext->getMsaaSamples(),
        colorFormat,
        VK_IMAGE_TILING_OPTIMAL,
        /**
        * VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT: 非常关键！ 这个标志告诉 Vulkan，这个图像的数据是“瞬时”的，它的生命周期只存在于一个渲染通道（Render Pass）内部。
        * 因为在开启 MSAA 时，我们会先渲染到这个多重采样图像，然后立即“解析”到单采样的交换链图像中。
        * 有了这个标志，GPU 会尝试将这个图像完全保存在**超高速的片上内存（Tile Memory / Cache）**中，而不会实际写入到慢速的常规显存（VRAM）中，这极大地节省了内存带宽并提升了性能。
        */
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        this->_multiSampleColorImage, 
        this->_multiSampleColorImageMemory,
        this->_vkContext->getLogicDevice(),
        this->_vkContext->getPhysicalDevice());
    this->_multiSampleColorImageView = vkDB::createImageView(
        this->_multiSampleColorImage,
        colorFormat, 
        VK_IMAGE_ASPECT_COLOR_BIT, 
        1,//多重采样缓冲区无需mipmap
        this->_vkContext->getLogicDevice());
}

//关键步骤：创建渲染通道--帧缓冲区附件--RenderPass
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
//createRenderPass中没有分配图像内存，没有创建真正的帧缓冲，只是在创建一个“蓝图”或者说是“规范说明书”
//可以理解为写职位描述 (Blueprint/规范)，后面的createFrame才是真正的创建帧缓冲；

//subpass规定了各种输入附件/输出附件，以及附件的图像布局，所以子通道是真正的渲染核心阶段，Subpass 是所有“绘制命令（Draw Call）”的家；
//需要为每个subpass创建图形管线，然后进行真正的绘制时进行绑定
void VulkanSwapChain::createRenderPass()
{
    //设置当前renderPass需要的帧缓冲附件，此处并没有真正的创建帧缓冲，只是标识出renderPass需要哪些帧缓冲附件，
    //后面的vkframeBuffer才是真正的创建对应于当前renderPass的帧缓冲

    //1-列出帧缓冲附件（Attachment），帧缓冲对应的显存（如颜色缓冲、深度缓冲）
    //---------------------------------------------------多重采样颜色缓冲区附件--------------------------------------------------------------------
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = this->_swapChainImageFormat; // 必须与交换链图像格式一致
    colorAttachment.samples = this->_vkContext->getMsaaSamples();// 使用多重采样 (MSAA)

    //loadOp/storeOp: 控制数据的读写。
    //CLEAR 确保每一帧开始都是干净的，STORE 确保我们画的东西被保留下来。
    // 渲染前：清除之前的图像内容 (类似 glClear)
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //绘制新帧：渲染后的内容将存储在内存中，稍后可以读取
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
    //colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;// 渲染结束后布局：转换为适合交换链呈现的格式//交换链中要展示的图像
    //多重采样缓冲区
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;// 渲染结束后布局：保持帧缓冲颜色附件布局，，That's because multisampled images cannot be presented directly

    //---------------------------------------------------深度缓冲区附件--------------------------------------------------------------------
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = this->_vkContext->getMsaaSamples();
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //绘制完深度缓冲区有没有用了，所以无需保存，除非像阴影贴图这种保存深度缓冲区的情况
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //VK_IMAGE_LAYOUT_UNDEFINED：无需知道之前的深度内容
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    //由于多重采样缓冲区附件无法直接用于显示（present），所以需要添加一个新的颜色缓冲区对应于交换链图片，用于交换链显示渲染
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = this->_swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    //绘制新帧--渲染后的内容将存储在内存中，稍后可以读取
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


    //2-定义附件引用--每个附件都需要定义一个附件引用
    //---------------------------------------------------颜色缓冲区附件引用--------------------------------------------------------------------
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;//使用索引为0的附件，即上面定义的VkAttachmentDescription colorAttachment
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//渲染过程中：引用的附件用作何种布局--这是一种为了“作为颜色缓冲区被写入”而极度优化的布局。

    //---------------------------------------------------深度缓冲区附件引用--------------------------------------------------------------------
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


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
    //输出附件
    subpass.pColorAttachments = &colorAttachmentRef;//多重采样缓冲区（样本数 = _msaaSamples）,GPU真正画图的地方
    subpass.pDepthStencilAttachment = &depthAttachmentRef;//深度缓冲区（样本数 = _msaaSamples）
    /**
    * 当你把 colorAttachmentResolveRef 赋值给 pResolveAttachments 时，就在给 GPU 下达一个底层指令：当这个子通道（Subpass）画完之后，
    * 请自动把 pColorAttachments（附件0，MSAA 高精度画面）里的多个采样点进行混合（抗锯齿处理），然后把最终结果**直接导出（Resolve）*到 pResolveAttachments（附件2，普通交换链图像）中！
    */
    subpass.pResolveAttachments = &colorAttachmentResolveRef;//用来显示的普通交换链图像（样本数 = VK_SAMPLE_COUNT_1_BIT）--这是 Vulkan 自动实现降采样（Resolve）的根本原因；


    /**
    * GPU 渲染像流水线（Pipeline）一样分为很多阶段（读取顶点 -> 顶点着色器 -> ... -> 颜色输出），
    * 因为GPU是并行工作的，如果不显式告诉它顺序，它可能会在上一帧图像还没显示完时，就开始往这张图里写新的数据，导致画面撕裂或崩溃。
    *
    * 子通道依赖项--控制“谁先做完，谁才能开始”，负责处理管线内部阶段的读写同步；
    * 子通道依赖并不能等到呈现引擎读取完毕，只能控制 vulkanPipline 内部的同步，vulkanPipline 使用信号量来等待呈现引擎读取完毕，呈现引擎不属于 vulkanPipline
    */

    //dstSubpass（消费者）运行到 dstStageMask 阶段必须进行等待，等待 srcSubpass（生产者）运行完毕 srcStageMask 阶段，dstStageMask 才能继续运行。
    //srcAccessMask 表示 src 运行完毕时对内存做了什么操作，而 dstAccessMask 表示 dst 等待结束开始运行时，要对内存做什么操作。


    VkSubpassDependency dependency{};
    //VK_SUBPASS_EXTERNAL 仅仅是一个**“时间边界代号”，因为当前程序是单renderPass操作，所以这里可以理解为上一帧的所有操作；
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;//指vkCmdBeginRenderPass（本渲染通道开始）之前提交给GPU的所有指令，包括等待呈现引擎的那个信号量--进入本次渲染通道之前的所有操作。
    dependency.dstSubpass = 0;//指的是我们代码里定义的第一个子通道
    //dependency.dstSubpass = VK_SUBPASS_EXTERNAL;//指vkCmdEndRenderPass（本渲染通道结束）之后提交给GPU的所有指令。

    //dstStageMask：表示消费者运行到dstStageMask阶段，必须等待
    //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ：运行到输出颜色阶段必须等待；
    //VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT：运行到提前深度测试阶段必须等待；
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    //srcStageMask：等待生产者完成srcStageMask阶段
    //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT：等待上一帧（生产者）输出颜色完成；
    //VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT（晚期片段测试）：等待上一帧（生产者）深度写入完成完成；
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    //控制内存的访问权限（Access/ 缓存刷新机制）
    //srcAccessMask ：要求 srcSubpass 上一帧（生产者）在解除阻塞之前，必须完成颜色和深度的“写入（WRITE）”操作彻底刷入物理显存（Make Available）
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //dstAccessMask ：表示依赖者 dstSubpass 在前置条件满足、解除阻塞开始运行时，明确告诉 GPU，我们的子通道接下来的操作是 写入 (WRITE) 颜色附件/深度附件，请确保我能看到刚才刷入显存的最新数据。
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //子通道依赖为什么没有设置等待呈现引擎读取扫描完毕呢？因为那是信号量的职责范围，子通道依赖无需设置；



    //5-创建渲染通道
    //附件和引用它的子通道都已描述完毕，开始创建渲染通道

    std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment ,colorAttachmentResolve };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    //把所有的附件描述放进一个数组std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    renderPassInfo.pAttachments = attachments.data();//附件数组--对应上面定义的附件，由于只有一个附件，直接赋值指针即可
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(this->_vkContext->getLogicDevice(), &renderPassInfo, nullptr, &this->_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

//关键步骤：创建帧缓冲区VkFramebuffer，VkFramebuffer不创建任何显存
/**
* vulkan帧缓冲与OpenGL的帧缓冲稍微有些不同，两者的一致之处都是将渲染结果绘制到某张中间图像上或者屏幕上（vulkan是交换链），
* 不同的是vulkan的帧缓冲绑定renderPass，调用的时候需要传入renderPass和vkFrameBuffer，而OpenGL中只要调用了glBindFrameBuffer，后面所有的操作都受其影响。
* vulkan需要为每个renderPass创建vkFrameBuffer，vkFrameBuffer链接vkImageView和renderPass，这样renderPass就可以绘制到对应的vkImageView
*/
//创建帧缓冲，将renderPass和vkImageView连接起来
/**
* VkImage (交换链图像) = 真正的、占体积的实物画板（真正消耗显存的地方）。
* VkImageView (图像视图) = 画板的使用说明书（说明它是 2D 的，颜色格式是 RGB 等）。
* VkRenderPass (渲染通道) = 厂长下发的工艺流程单（要求员工：开始前洗干净画板，画完后存好，并且规定了需要几张画板配合工作）。
* VkFramebuffer (帧缓冲) = 一个回形针,把**“工艺流程单（RenderPass）”、“具体的画板使用说明（ImageView）”，以及“这次画画的尺寸（宽、高）”**，全部装订在一起，变成一份完整的工作包（Job Packet）
*/
//_renderPass描述画图的规则，需要的附件/工具/流程，ImageViews描述真实的显存，VkFramebuffer将两者链接起来，链接过程务必一一对应。
//VkFramebuffer规定：按照_renderPass的流程，往_swapChainImageViews[i] 指向的那块物理显存里，在这个宽高范围内画图。
//在 Vulkan 中，VkFramebuffer 的本质作用是：把一组实际的 VkImageView（图像视图）打包在一起，提供给 VkRenderPass（渲染通道）作为渲染目标（Render Targets）。
void VulkanSwapChain::createFramebuffers()
{
    //调整容器大小，使其能够容纳所有的帧缓冲区
    this->_swapChainFramebuffers.resize(this->_swapChainImageViews.size());
    //遍历图像视图并从中创建帧缓冲区
    for (size_t i = 0; i < this->_swapChainImageViews.size(); i++) {

        /**
        * 1-顺序（Order）严格对应：
        * framebufferInfo.pAttachments是真实的图像，renderPassInfo.pAttachments是附件描述，
        * framebufferInfo.pAttachments要与renderPassInfo.pAttachments完全对应，
        * 如果 RenderPass 的数组是：[0] 颜色附件, [1] 深度附件
        * 那么 Framebuffer 的数组必须是：[0] 颜色视图, [1] 深度视图
        * 2-格式（Format）：100% 严格对应：
        * RenderPass 里说索引 0 的附件格式是 VK_FORMAT_B8G8R8A8_SRGB。
        * 那么 Framebuffer 传进来的索引 0 的 VkImageView，在它创建时的格式也必须完全等于 VK_FORMAT_B8G8R8A8_SRGB。
        * 3. 多重采样数（Samples）：100% 严格对应：
        * RenderPass 里要求 samples = VK_SAMPLE_COUNT_1_BIT。
        * 那么传入的 VkImageView 底层图像的采样数也必须是 1。如果你传入了一个 4 倍抗锯齿的图像，校验层会直接报错，
        * 目前直接使用交换链_swapChainImage，所有用于直接输出到屏幕的交换链图像，其采样率必须、也只能是 1 倍（VK_SAMPLE_COUNT_1_BIT），所以与RenderPass里要求的samples对应起来
        */
        std::array<VkImageView, 3> attachments = { this->_multiSampleColorImageView, this->_depthImageView ,this->_swapChainImageViews[i] };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        //链接renderPass
        framebufferInfo.renderPass = this->_renderPass;//frameBuffer绑定的renderPass
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());

        //链接vkImageView（真实显存）
        framebufferInfo.pAttachments = attachments.data();//vkFrameBuffer绑定到vkImageView（真实的显存信息）,这儿是直接绑定到交换链，那当前renderPass会直接往屏幕上进行绘制
        framebufferInfo.width = this->_swapChainExtent.width;
        framebufferInfo.height = this->_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(this->_vkContext->getLogicDevice(), &framebufferInfo, nullptr, &this->_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}
