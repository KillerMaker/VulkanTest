#pragma once
#include <fstream>
#include<glm/glm.hpp>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif 
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const int MAX_FRAME_COUNT = 2;

//Indices (Locations) of queue families
struct QueueFamilyIndices
{
	int graphicsFamily = -1;		//Location of graphics family
	int presentationFamily = -1;	//Location of presentation Family

	bool isValid()
	{
		return graphicsFamily >= 0 && presentationFamily >=0;
	}
};

struct VertexData {
	glm::vec3 position;
	glm::vec3 color;
};

struct Device
{
	VkPhysicalDevice physicalDevice{};
	VkDevice logicalDevice{};
};

struct SwapChainDetails 
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	std::vector<VkPresentModeKHR> surfacePresentationModes;

	bool isValid()
	{
		return 
			&surfaceCapabilities 
			&& surfaceFormats.size() > 0 
			&& surfacePresentationModes.size() > 0;
	}
};

struct SwapChainImage {
	VkImage image;
	VkImageView imageView;
};



static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	if (!file.is_open())
		throw std::runtime_error("Failed to open a file!");

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	file.seekg(0);

	file.read(fileBuffer.data(), fileSize);

	file.close();

	return fileBuffer;
}