#include "vulkanBuffer.h"
#include "vulkanContext.h"
#include "vulkanCommandManager.h"
#include "readBuffer.h"

// deviceSize: 缓冲区大小
// usageFlags: 用途 (例如 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
// memoryPropertyFlags: 显存特性 (例如 CPU可见、GPU专享等)//显存类型/属性（专用显存/共享显存）
VulkanBuffer::VulkanBuffer(
    std::shared_ptr<VulkanContext> context,
    VkDeviceSize deviceSize,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags memoryPropertyFlags)
{
    this->_vkContext = context;
    this->_bufferSize = deviceSize;
    this->_usageFlags = usageFlags;
    this->_memoryPropertyFlags = memoryPropertyFlags;
    this->createBuffer();
}


VulkanBuffer::~VulkanBuffer()
{
    this->unmap();

    //销毁 Buffer 对象
    if (this->_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(this->_vkContext->getLogicDevice(), this->_buffer, nullptr);
        this->_buffer = VK_NULL_HANDLE;
    }

    //释放底层显存
    if (this->_bufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(this->_vkContext->getLogicDevice(), this->_bufferMemory, nullptr);
        this->_bufferMemory = VK_NULL_HANDLE;
    }
}


void VulkanBuffer::createBuffer()
{
    //1-创建缓冲区对象，只定义，不占显存，描述vkBuffer状态信息：大小/用途等
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = this->_bufferSize;
    //指示缓冲区中的数据用途
    bufferInfo.usage = this->_usageFlags;
    //缓冲区也可以由特定的队列族拥有，或者同时在多个队列族之间共享。缓冲区只会从图形队列中使用，因此我们可以坚持独占访问
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(this->_vkContext->getLogicDevice(), &bufferInfo, nullptr, &this->_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }
    //2-内存需求--根据上述描述查询其内存需求，询问显卡：刚才创建的这个 buffer，需要分配多少内存？需要什么样的对齐方式？支持哪种内存类型？
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->_vkContext->getLogicDevice(), this->_buffer, &memRequirements);

    //3-内存（显存）分配
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;// 注意：分配的大小可能比 bufferInfo.size 大（因为显存需要对齐）
    // 寻找合适的内存类型。核心诉求：
    // 1. VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT：CPU 可以通过映射(Mapping)访问这块内存。
    // 2. VK_MEMORY_PROPERTY_HOST_COHERENT_BIT：内存连贯性。保证 CPU 写入后 GPU 立即能看到，不需要手动调用 vkFlushMappedMemoryRanges 刷新缓存。
    allocInfo.memoryTypeIndex = vkDB::findMemoryType(memRequirements.memoryTypeBits, this->_memoryPropertyFlags, this->_vkContext->getPhysicalDevice());

    if (vkAllocateMemory(this->_vkContext->getLogicDevice(), &allocInfo, nullptr, &this->_bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
    //4-缓冲区与内存进行绑定--第四个参数：内存区域内的偏移量
    vkBindBufferMemory(this->_vkContext->getLogicDevice(), this->_buffer, this->_bufferMemory, 0);

}


void VulkanBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
    // 如果已经映射过了，且传进来的参数没变，可以直接返回，避免重复开销
    if (this->_mappedData != nullptr) return;
    
    if (size == VK_WHOLE_SIZE) {
        size = this->_bufferSize;
    }
    if (vkMapMemory(this->_vkContext->getLogicDevice(), this->_bufferMemory, offset, size, 0, &this->_mappedData) != VK_SUCCESS) {
        throw std::runtime_error("failed to map buffer memory!");
    }
}

void VulkanBuffer::writeToBuffer(void* srcData, VkDeviceSize size, VkDeviceSize offset) {
    // 1. 如果 size 是 VK_WHOLE_SIZE，表示用户想写满整个 Buffer
    if (size == VK_WHOLE_SIZE) {
        size = this->_bufferSize;
    }

    // 2. 检查越界
    // 如果 offset + size > this->_bufferSize，说明用户想写的数据比 Buffer 还要大，必须抛出异常或报错。
    if(offset + size > this->_bufferSize)
        throw std::runtime_error("out of memory range!");


    // 3. 映射内存
    // 如果 this->_mappedData 是 nullptr，说明还没映射过，我们需要调用 map()。
    if (this->_mappedData == nullptr) {
        this->map(size, offset); // 这会调用 vkMapMemory 拿到指针对 _mappedData 赋值
    }

    // 4. 拷贝数据
    // 注意：我们要拷贝到的目标地址是 _mappedData 加上偏移量 offset
    char* targetAddress = static_cast<char*>(this->_mappedData);
    // 这里也可以加上 offset，具体看您的 map 函数是怎么实现的，通常 map(size, offset) 后返回的指针就是目标地址，如果 map 的是整体，写入时就加上 offset。
    memcpy(targetAddress, srcData, static_cast<size_t>(size));

    // 5. 【可选且高级】处理内存非连贯的情况
    // 如果您在分配内存时没有指定 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    // 您必须在这里执行：
    // VkMappedMemoryRange mappedRange{};
    // mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    // mappedRange.memory = this->_bufferMemory;
    // mappedRange.offset = offset;
    // mappedRange.size = size;
    // vkFlushMappedMemoryRanges(this->_vkContext->getLogicDevice(), 1, &mappedRange);
}

void VulkanBuffer::unmap()
{
    if (this->_mappedData) {
        vkUnmapMemory(this->_vkContext->getLogicDevice(), this->_bufferMemory);
    }
    this->_mappedData = nullptr;
}


void VulkanBuffer::copyBuffer(
    std::shared_ptr<VulkanCommandManager> commandManager,
    VkBuffer dstBuffer, 
    VkDeviceSize size) {
    // 1. 如果 size 是 VK_WHOLE_SIZE，表示用户想写满整个 Buffer
    if (size == VK_WHOLE_SIZE) {
        size = this->_bufferSize;
    }

    //内存传输也需要命令缓冲区
    VkCommandBuffer commandBuffer = commandManager->beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, this->_buffer, dstBuffer, 1, &copyRegion);
    commandManager->endSingleTimeCommands(commandBuffer);
}