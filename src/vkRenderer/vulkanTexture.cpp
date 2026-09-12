#include "vulkanTexture.h"
#include "vulkanBuffer.h"
#include "vulkanContext.h"
#include "vulkanCommandManager.h"
#include "readBuffer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

VulkanTexture::VulkanTexture(
    std::shared_ptr<VulkanContext> context,
    std::shared_ptr<VulkanCommandManager> commandManager,
    const std::string& filePath
)
{
    this->_vkContext = context;
    this->_vkCommandManager = commandManager;
    this->_imageFilePath = filePath;
    this->createTextureImage();
    this->createTextureSampler();
}


VulkanTexture::~VulkanTexture()
{
    if(this->_vkSampler)
        vkDestroySampler(this->_vkContext->getLogicDevice(), this->_vkSampler, nullptr);
    if (this->_vkImageView)
        vkDestroyImageView(this->_vkContext->getLogicDevice(), this->_vkImageView, nullptr);
    if (this->_vkImage)
        vkDestroyImage(this->_vkContext->getLogicDevice(), this->_vkImage, nullptr);
    if (this->_vkImageMemory)
        vkFreeMemory(this->_vkContext->getLogicDevice(), this->_vkImageMemory, nullptr);
}


void VulkanTexture::createTextureImage()
{
    int texWidth, texHeight, texChannels;
    //STBI_rgb_alpha--强制加载带有 alpha 通道的图像，即使图像本身没有 alpha 通道，这有助于将来与其他纹理保持一致
    //stbi_uc* pixels = stbi_load((exeDir + "/resources/images/texture.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    stbi_uc* pixels = stbi_load(this->_imageFilePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    //std::floor根据最大维度向下取整，最后加1是加上图片自身的那一层，比如图片4*4/2*2/1*1，mipmap：2 + 1 = 3
    this->_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    

    //1-创建暂存缓冲区（显存），存储image对应的数据（内存数据复制到显存）
    // 1. VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT：就是任务管理器的显示共享GPU内存（其实就是内存），CPU 可以通过映射(Mapping)访问这块内存。
    // 2. VK_MEMORY_PROPERTY_HOST_COHERENT_BIT：内存连贯性。保证 CPU 写入后 GPU 立即能看到，不需要手动调用 vkFlushMappedMemoryRanges 刷新缓存。
    VulkanBuffer vulkanBuffer(
        this->_vkContext, 
        this->_vkCommandManager,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vulkanBuffer.setBufferData(pixels, static_cast<size_t>(imageSize), 0.0);
    stbi_image_free(pixels);

    /*createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    void* data;
    vkMapMemory(this->_logicDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(this->_logicDevice, stagingBufferMemory);
    stbi_image_free(pixels);*/

    //2-创建图像对象并分配显存（包括mipmap层的显存）
    //VK_IMAGE_TILING_LINEAR（线性布局）,纹理元素按照行优先顺序排列，就像我们的 pixels数组一样，CPU 可以直接理解这种格式,GPU 访问效率非常低;
    //VK_IMAGE_TILING_OPTIMAL（最优布局 / 瓦片布局）, 像素在内存中是以显卡厂商私有的、优化过的块状（Block / Tile）方式存放的, GPU 访问效率极高！
    //VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT，显卡专用内存，GPU读取非常快
    vkDB::createImage(
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight),
        this->_mipLevels,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,//图像的内存排列方式
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | //图像用途-图像是传输的源：用于生成mipmap
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | //图像用途-图像是传输的目标：接受暂存缓冲区的数据
        VK_IMAGE_USAGE_SAMPLED_BIT,//声明这个图像（Image）将来会被着色器（Shader）作为纹理进行采样（读取）。
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        this->_vkImage,
        this->_vkImageMemory,
        this->_vkContext->getLogicDevice(),
        this->_vkContext->getPhysicalDevice());
    //将暂存缓冲区复制到纹理图像
    //第一步：将纹理图像过渡到VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL布局，所有的mipmap层也都转化成了VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL布局，准备接收数据
    vkDB::transitionImageLayout(this->_vkImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, this->_mipLevels, this->_vkCommandManager.get());
    //第二步：执行缓冲区到图像的复制操作，暂存缓冲区只能用于填充mip级别0，mipmap层并没有被填充（但是mipmap层显存已经创建好了），需要手动生成mipmap层
    vulkanBuffer.copyToImage(this->_vkImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));


    //最后清理暂存缓冲区
    //vkDestroyBuffer(this->_logicDevice, stagingBuffer, nullptr);
    //vkFreeMemory(this->_logicDevice, stagingBufferMemory, nullptr);

    //Vulkan 允许我们独立地转换图像的每个 mip 级别，每次 blit 操作一次只会处理两个 mip 级别，因此我们可以在两次 blit 命令之间将每个级别转换为最佳布局。
    // 这将使纹理图像的每个层级都保留在。每个层级在 blit 命令读取完成后都会由VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL过渡到VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
    this->generateMipmaps(VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, this->_mipLevels);
    //创建图像视图
    this->_vkImageView = vkDB::createImageView(this->_vkImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, this->_mipLevels,this->_vkContext->getLogicDevice());
}

void VulkanTexture::createTextureSampler()
{
    vkDB::createTextureSampler(
        this->_vkSampler,
        this->_vkContext->getLogicDevice(),
        this->_vkContext->getPhysicalDevice());
}

void VulkanTexture::generateMipmaps(
    VkFormat imageFormat,
    int32_t texWidth,
    int32_t texHeight,
    uint32_t mipLevels)
{
    // Check if image format supports linear blitting

       /**VkFormatProperties:
        * linearTilingFeatures线性平铺支持的用例
        * optimalTilingFeatures：支持最佳平铺效果的使用场景。
        * bufferFeatures缓冲区支持的用例
        */
        //结构体包含了显卡对某一种图像格式（imageFormat）支持的所有功能集。
    VkFormatProperties formatProperties;
    //vkGetPhysicalDeviceFormatProperties--询问显卡（_physicalDevice）：“对于 imageFormat 这个格式，都能对它做哪些操作
    vkGetPhysicalDeviceFormatProperties(this->_vkContext->getPhysicalDevice(), imageFormat, &formatProperties);
    //image图片是optimalTiling内存排列方式，所以需要检查formatProperties.optimalTilingFeatures
    //VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT-代表支持线性滤波
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }


    VkCommandBuffer commandBuffer = this->_vkCommandManager->beginSingleTimeCommands();

    //配置内存屏障：内存屏障 (Barrier) 在 Vulkan 中有两个主要作用：同步内存访问和转换图像布局 (Image Layout)。
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = this->_vkImage;//处理的目标图像
    //忽略队列组--如果是队列族所有权转换，那将是队列族索引
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //subresourceRange 指定了这个屏障作用于图像的哪个部分。
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//表明这是一个颜色图像（而不是深度/模板图像）。
    barrier.subresourceRange.baseArrayLayer = 0;//表明只处理图像的第 1 层（这不是纹理数组或立方体贴图）
    barrier.subresourceRange.layerCount = 1;//表明只处理图像的第 1 层（这不是纹理数组或立方体贴图）
    barrier.subresourceRange.levelCount = 1;//在生成 Mipmap 的过程中，需要逐级处理，会把第 i−1级作为读取源（Source），把第i级作为写入目标（Destination），因此屏障每次只针对单独的一级 Mipmap 进行布局转换

    //屏障主要用于同步，因此必须指定哪些涉及资源的操作必须在屏障之前执行（srcAccessMask），以及哪些涉及资源的操作必须等待屏障执行（dstAccessMask）
    //barrier.srcAccessMask:表示sourceStage 阶段中已经做了什么类型的内存读写操作，    
    //barrier.dstAccessMask:表示进入 destinationStage 阶段后，将会做什么类型的内存读写操作？

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        //第一步将上层源数据转化为VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL布局，以便作为mipmap数据源使用
        barrier.subresourceRange.baseMipLevel = i - 1;//i - 1层，即上一层的源数据
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;//初始的图像被创建且数据数据填充后，mipmap层都是VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL布局
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;//转化到源数据布局
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        //This transition will wait for level i - 1 to be filled, either from the previous blit command, or from vkCmdCopyBufferToImage.
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        //指定mipmap源数据（i-1层），已转化为VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL布局
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;

        //指定mipmap写入目标（i层），第i层仍然是VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL布局
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            this->_vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            this->_vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);//使用与创建vkSampler相同的过滤方式

        //i-1层转换为VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL布局，供着色器使用
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;//原布局是VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL（即数据源，被读取目标），所以转换之前要保证被读取完成
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        //This transition waits on the current blit command to finish. 
        vkCmdPipelineBarrier(commandBuffer,
            //srcStageMask阶段：源阶段，必须等待完成的管线阶段，它约束的是 Barrier 之前的命令,在 Barrier 生效之前，必须等待之前提交的所有命令，执行完 srcStageMask 所指定的阶段。
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            //dstStageMask阶段：目标阶段，必须阻塞的管线阶段，它约束的是 Barrier 之后的命令，当执行到 dstStageMask 所指定的阶段时，必须停下来等待，直到 Barrier 前面的要求被满足。
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    //最后一层mipmap布局转换
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
    this->_vkCommandManager->endSingleTimeCommands(commandBuffer);
}

