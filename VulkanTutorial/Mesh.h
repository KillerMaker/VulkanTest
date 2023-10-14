#pragma once

#include<vulkan/vulkan.h>
#include<GLFW/glfw3.h>
#include<vector>
#include "Utilities.h"

class Mesh
{
private:
	size_t vertexCount;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexMemory;

	VkPhysicalDevice physicalDevice;
	VkDevice device;

	void creaeVertexBuffer(std::vector<VertexData>& vertices);

public:

	Mesh() = default;

	Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<VertexData>& vertices);

	int getVerticesCount();

	VkBuffer getVertexBuffer();

	void destroyVertexBuffer();
	
	uint32_t findMemoryIndex(uint32_t allowedTypes, VkMemoryPropertyFlags flags);
};

