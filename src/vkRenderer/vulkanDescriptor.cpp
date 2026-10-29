#include "vulkanDescriptor.h"
#include "vulkanContext.h"

VulkanDescriptorManager::VulkanDescriptorManager(std::shared_ptr<VulkanContext> context)
{
	this->_vkContext = context;
}

VulkanDescriptorManager::~VulkanDescriptorManager() {
    // 销毁所有创建过的池子
    for (auto p : this->_freePools) {
        vkDestroyDescriptorPool(this->_vkContext->getLogicDevice(), p, nullptr);
    }
    for (auto p : this->_usedPools) {
        vkDestroyDescriptorPool(this->_vkContext->getLogicDevice(), p, nullptr);
    }
    if (this->_currentPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(this->_vkContext->getLogicDevice(), this->_currentPool, nullptr);
    }
}

VkDescriptorPool VulkanDescriptorManager::createPool() {
    // 设置通用比例，足以应对绝大多数场景
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0; // 不使用 FREE_DESCRIPTOR_SET_BIT，因为我们统一 reset
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    if (vkCreateDescriptorPool(this->_vkContext->getLogicDevice(), &poolInfo, nullptr, &newPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
    return newPool;
}

//从空闲池子获取一个pool
VkDescriptorPool VulkanDescriptorManager::grabPool() {
    if (!this->_freePools.empty()) {
        VkDescriptorPool pool = this->_freePools.back();
        this->_freePools.pop_back();
        return pool;
    }
    else {
        return createPool();
    }
}

void VulkanDescriptorManager::resetPools() {
    // 重置所有 used 和 current 的池子，并将它们放入 free 队列以供复用
    for (auto p : this->_usedPools) {
        vkResetDescriptorPool(this->_vkContext->getLogicDevice(), p, 0);
        this->_freePools.push_back(p);
    }
    this->_usedPools.clear();

    if (this->_currentPool != VK_NULL_HANDLE) {
        vkResetDescriptorPool(this->_vkContext->getLogicDevice(), this->_currentPool, 0);
        this->_freePools.push_back(this->_currentPool);
        this->_currentPool = VK_NULL_HANDLE;
    }
}


// 核心功能：尝试分配一个描述符集。如果当前池满了，自动创建新池。
bool VulkanDescriptorManager::allocate(VkDescriptorSet& descriptorSets, VkDescriptorSetLayout layout)
{
    if (!this->_currentPool)
        this->_currentPool = this->grabPool();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pSetLayouts = &layout;
    allocInfo.descriptorPool = this->_currentPool;
    //allocInfo.descriptorSetCount必须小于poolInfo.maxSets的剩余量
    allocInfo.descriptorSetCount = 1;

    // 尝试分配
    VkResult allocResult = vkAllocateDescriptorSets(this->_vkContext->getLogicDevice(), &allocInfo, &descriptorSets);
    bool needReallocate = false;

    switch (allocResult) {
    case VK_SUCCESS:
        return true;
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        needReallocate = true;
        break;
    default:
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
            return false; // 其他致命错误
        }
    }

    if (needReallocate) {
        // 当前池满了，把它丢进已使用队列，换一个新池子继续！
        this->_usedPools.push_back(this->_currentPool);
        this->_currentPool = this->grabPool();
        allocInfo.descriptorPool = this->_currentPool;

        if (vkAllocateDescriptorSets(this->_vkContext->getLogicDevice(), &allocInfo, &descriptorSets) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
            return false;
        }
    }
    return true;
}


VulkanDescriptorBuilder::VulkanDescriptorBuilder(
    std::shared_ptr<VulkanContext> context, 
    std::shared_ptr<VulkanDescriptorManager> allocator)
{
    this->_vkContext = context;
    this->_allocator = allocator;
}

VulkanDescriptorBuilder::~VulkanDescriptorBuilder()
{
    this->_bufferInfos.clear();
    this->_imageInfos.clear();
}


void VulkanDescriptorBuilder::bindData(
    uint32_t binding,//几号描述符
    VkDescriptorType type,//资源类型（ubo/ssbo/sampler）
    VkShaderStageFlags stageFlags,//shader使用阶段
    const VkDescriptorBufferInfo* bufferInfo,//对应的真实buffer信息
    const VkDescriptorImageInfo* imageInfo)
{
    if (!bufferInfo && !imageInfo)
        return;

    if(bufferInfo)
        this->_bufferInfos.push_back(*bufferInfo);
    if (imageInfo)
        this->_imageInfos.push_back(*imageInfo);

    //编号为binding的描述符说明书
    VkDescriptorSetLayoutBinding newLayoutBinding{};
    //1-binding = 0：槽位编号（插孔编号），这严格对应了在 GLSL 着色器代码里写的 layout(binding = 0)
    newLayoutBinding.binding = binding;
    //2-descriptorType：描述符类型（UBO）
    newLayoutBinding.descriptorType = type;
    //descriptorCount表示 layout(binding = X) 这个槽位需要创建多少个描述符，一个描述符就是一个指针，它只能指向 1 块具体的显存资源（1 个 Buffer 或者 1 张 Texture）。
    //layout(set = 0, binding = 1) uniform sampler2D textures[10];就需要descriptorCount= 10，表示创建10个描述符，指向十块指针内存
    //这里只是单一的 UBO，所以是 descriptorCount = 1，如果你在着色器里声明了一个 UBO 数组，这里就填数组长度。。
    newLayoutBinding.descriptorCount = 1;
    //4-指定ubo对象用于哪个着色器阶段
    newLayoutBinding.stageFlags = stageFlags;
    //用于image sampling
    newLayoutBinding.pImmutableSamplers = nullptr; // Optional
    this->_bindings.push_back(newLayoutBinding);


    //编号为binding的描述符说明书，对应链接的真实显存数据
    VkWriteDescriptorSet newDescriptorWrites{};
    newDescriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newDescriptorWrites.descriptorType = type;
    // dstSet 需要更新的描述符集
    //descriptorWrites.dstSet = this->_descriptorSets[i];
    newDescriptorWrites.dstBinding = binding;//更新对应的槽位binding--layout(binding = 0)
    //可以一次性更新数组中的多个描述符，从索引dstArrayElement开始，该descriptorCount字段指定要更新的描述符数组元素数量。
    newDescriptorWrites.dstArrayElement = 0;//描述符可以是数组，因此需要指定要更新的描述符数组中的初始索引
    //如果0号槽位对应十个描述符，descriptorWrite.descriptorCount= 10
    newDescriptorWrites.descriptorCount = 1;
    if (bufferInfo)
        newDescriptorWrites.pBufferInfo = &(this->_bufferInfos.back());;//链接真实数据
    if (imageInfo)
        newDescriptorWrites.pImageInfo = &(this->_imageInfos.back()); //链接真实数据
    newDescriptorWrites.pTexelBufferView = nullptr; //链接真实数据
    this->_writes.push_back(newDescriptorWrites);
}


// 绑定一个 Buffer 资源（例如 UBO 或 SSBO）
void VulkanDescriptorBuilder::bindBuffer(
    uint32_t binding,//几号描述符
    VkDescriptorType type,
    VkShaderStageFlags stageFlags,
    const VkDescriptorBufferInfo* bufferInfo//对应的显存数据
)
{
    this->bindData(binding, type, stageFlags, bufferInfo, nullptr);
}


// 绑定一个 Image 资源（例如 纹理贴图）
void VulkanDescriptorBuilder::bindImage(
    uint32_t binding,//几号描述符
    VkDescriptorType type,
    VkShaderStageFlags stageFlags,
    const VkDescriptorImageInfo* imageInfo//对应的显存数据
)
{
    this->bindData(binding, type, stageFlags, nullptr, imageInfo);
}


//VkDescriptorSetLayout 描述符集说明书，未来创建的描述集中总共包含多少个描述符（this->_bindings）（比如：0号描述符ubo/1号ssbo/2号sampler等）
//VkWriteDescriptorSet 每种描述符对应的真实显存地址，用于更新描述符集中的描述符（0号ubo对应的显存/1号ssbo对应的显存/2号sampler对应的显存）
//VkDescriptorSet 根据说明书创建的真实描述符集合，包含说明书中的所有描述符
bool VulkanDescriptorBuilder::build(
    VkDescriptorSet& descriptorSet, 
    VkDescriptorSetLayout& descriptorSetLayout)
{
    //创描述符说明书
    if (!descriptorSetLayout)
    {
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(this->_bindings.size());
        layoutInfo.pBindings = this->_bindings.data();

        //this->_descriptorSetLayout是上面 layout(binding = X)槽位的集合
        if (vkCreateDescriptorSetLayout(this->_vkContext->getLogicDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
            this->reset();
            return false;
        }
    }
    //根据说明书创建描述符集
    if (this->_allocator->allocate(descriptorSet, descriptorSetLayout))
    {
        for (size_t i = 0; i < this->_writes.size(); i++)
        {
            this->_writes.at(i).dstSet = descriptorSet;
        }
        vkUpdateDescriptorSets(this->_vkContext->getLogicDevice(), static_cast<uint32_t>(this->_writes.size()), this->_writes.data(), 0, nullptr);
        this->reset();
        return true;
    }
    this->reset();
    return false;
}

void VulkanDescriptorBuilder::reset()
{
    this->_bufferInfos.clear();
    this->_imageInfos.clear();

    this->_bindings.clear();
    this->_writes.clear();
}