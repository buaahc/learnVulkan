#include "vulkanContext.h"
#include "vulkanCommandManager.h"

VulkanCommandManager::VulkanCommandManager(std::shared_ptr<VulkanContext> context)
{
	this->_vkContext = context;
}

void VulkanCommandManager::createCommandPool()
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
    * 限制：一个命令池只能为特定的一种队列族创建命令缓冲区，_graphicsAndComputeFamily 这个队列族即可以绘制图形又可以用于计算着色器。
    * 这里使用了 _graphicsFamily（图形队列族），说明从这个池里创建出来的命令，将来是发给 GPU 用来画图的。
    */
    poolInfo.queueFamilyIndex = this->_vkContext->getPhysicalQueueFamilyIndices()._graphicsAndComputeFamily.value();
    //创建命令池
    if (vkCreateCommandPool(this->_vkContext->getLogicDevice(), &poolInfo, nullptr, &this->_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

std::vector<VkCommandBuffer>  VulkanCommandManager::createCommandBuffers(uint32_t count, VkCommandBufferLevel level) {
    //1-分配而非创建：
    //Create 通常意味着从系统申请内存，开销较大。
    //Allocate 意味着从已经存在的资源池（Command Pool）中“拨”出一块内存。这样非常高效，因为命令池已经预先管理好了内存。

    std::vector<VkCommandBuffer> commandBuffers(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = this->_commandPool;
    /**
    *  VK_COMMAND_BUFFER_LEVEL_PRIMARY:主命令缓冲区--它可以被直接提交到 GPU 队列（Queue）执行，它可以执行（调用）二级命令缓冲区。
    *  VK_COMMAND_BUFFER_LEVEL_SECONDARY:二级命令缓冲区--不能直接提交给GPU，但可以被主命令缓冲区调用（类似于函数调用）。
    */
    //设置为分配主命令缓冲区
    allocInfo.level = level;
    allocInfo.commandBufferCount = commandBuffers.size();//指定一次性分配多少个缓冲区
    //执行分配--分配完成后_commandBuffer句柄就可用了，但注意：此时的命令缓冲区是空的，而且处于“初始状态（Initial state）”。你还不能把它交给 GPU。
    //vkAllocateCommandBuffers一次执行可以分配多个命令缓冲区
    if (vkAllocateCommandBuffers(this->_vkContext->getLogicDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    return commandBuffers;
}

VkCommandBuffer VulkanCommandManager::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = this->_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(this->_vkContext->getLogicDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;//命令缓冲区进行传输工作，且只会使用一次命令缓冲区，并且会等待函数执行完毕后再返回

    //记录命令缓冲区
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    return commandBuffer;
}

void VulkanCommandManager::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    //提交到绘制队列
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    //发送到GPU绘制队列准备执行
    vkQueueSubmit(this->_vkContext->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    //强制阻塞等待执行完毕
    vkQueueWaitIdle(this->_vkContext->getGraphicsQueue());
    //清理用于传输操作的命令缓冲区
    vkFreeCommandBuffers(this->_vkContext->getLogicDevice(), this->_commandPool, 1, &commandBuffer);
}

void VulkanCommandManager::cleanup() {
    //请记住，当释放命令池时，命令缓冲区也会被释放，因此无需对命令缓冲区进行任何额外的清理工作。
    vkDestroyCommandPool(this->_vkContext->getLogicDevice(), this->_commandPool, nullptr);

}