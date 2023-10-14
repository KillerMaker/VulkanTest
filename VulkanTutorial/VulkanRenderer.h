#pragma once

#include<vulkan/vulkan.h>
#include<GLFW/glfw3.h>
#include<stdexcept>
#include<vector>
#include<iostream>
#include<algorithm>
#include<string>
#include<set>
#include"Utilities.h"
#include"VulkanValidation.h"
#include<array>
#include"Mesh.h"

class VulkanRenderer
{
public:
	VulkanRenderer() = default;
	int init(GLFWwindow* window);
	void draw();
	void cleanup() noexcept;
	~VulkanRenderer();

private:
	int currentFrame = 0;
	
	std::vector<Mesh> meshes;

	GLFWwindow* window;

	//Vulkan Components
	VkInstance instance;
	Device mainDevice;
	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkExtent2D swapChainExtent;
	VkFormat swapChainFormat;

	VkDebugReportCallbackEXT callback;

	std::vector<SwapChainImage> swapChainImages;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	//Pipeline
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	//Pools
	VkCommandPool graphicsCommandPool;

	//Syncronization
	std::vector<VkSemaphore> readyToDraw;
	std::vector<VkSemaphore> readyToPresent;
	std::vector<VkFence> drawFences;

	//Vulkan Functions
	//***********************CREATE FUNCTIONS*********************************
	void createVkInstance();
	void createDebugCallback();
	void createLogicalDevice();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSyncronization();

	//**********************RECORD FUNCTIONS***********************************
	void recordCommands();

	//***********************CHECKER FUNCTIONS*********************************
	bool checkInstanceExtensionSupport(const std::vector<const char*>& extensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensions(VkPhysicalDevice device);
	bool checkValidationLayerSupport();

	//************************GET FUNCTIONS*************************************
	void getPhysicalDevice();
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	SwapChainDetails getSwapchainDetails(VkPhysicalDevice device);

	//************************CHOOSE FUNCTIONS**********************************
	VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
	VkPresentModeKHR choosePresentationMode(const std::vector<VkPresentModeKHR>& surfacePresentationModes);
	VkExtent2D chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	//**************************CREATE SUPPORT FUNCTIONS****************************
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags);
	VkShaderModule createShaderModule(const std::vector<char>& code);
};

