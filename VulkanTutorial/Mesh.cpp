#include "Mesh.h"

void Mesh::creaeVertexBuffer(std::vector<VertexData>& vertices)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.pNext = nullptr;
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = sizeof(VertexData) * vertexCount;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &vertexBuffer);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create vertex buffer!");

	VkMemoryRequirements memReqs = {};
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memReqs);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = nullptr;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = findMemoryIndex(memReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	result = vkAllocateMemory(device, &memAllocInfo, nullptr, &vertexMemory);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate vertex buffer memory!");

	vkBindBufferMemory(device, vertexBuffer, vertexMemory, 0);

	void* data = 0x00000000;

	vkMapMemory(device, vertexMemory, 0, memAllocInfo.allocationSize, 0, &data);

	memcpy(data, vertices.data(), (size_t)memAllocInfo.allocationSize);

	vkUnmapMemory(device, vertexMemory);
}

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<VertexData>& vertices) :
	vertexCount{ vertices.size() }, physicalDevice{ physicalDevice }, device{ device } {
	creaeVertexBuffer(vertices);
}

int Mesh::getVerticesCount()
{
    return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

void Mesh::destroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexMemory, nullptr);
}

uint32_t Mesh::findMemoryIndex(uint32_t allowedTypes, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties physicalMemProps = {};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalMemProps);

	for (uint32_t i = 0; i < physicalMemProps.memoryTypeCount; i++)
		if ((allowedTypes & (1 << i)) && physicalMemProps.memoryTypes[i].propertyFlags == flags)
			return i;
}
