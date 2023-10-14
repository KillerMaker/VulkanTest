#include "VulkanRenderer.h"

int VulkanRenderer::init(GLFWwindow* window)
{
    this->window = window;

	try
	{
		createVkInstance();
		createDebugCallback();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();

		auto vertices = std::vector<VertexData>{
			VertexData{{0.0f,-0.1f,0.0f}, {1.0f, 0.0f, 0.0f}},
			VertexData{{0.1f, 0.1f,0.0f}, {0.0f, 1.0f, 0.0f}},
			VertexData{{-0.1f,0.1f,0.0f}, {0.0f, 0.0f, 1.0f}}
		};

		meshes = { 
			Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, vertices)
		};

		createSwapChain();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		recordCommands();
		createSyncronization();
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "ERROR:" << e.what() << "\n";
		return EXIT_FAILURE;
	}
    
    return EXIT_SUCCESS;
}

void VulkanRenderer::draw()
{
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_FALSE, std::numeric_limits<uint64_t>::max());
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	uint32_t imageIndex = 0;
	VkResult result;

	vkAcquireNextImageKHR(
		mainDevice.logicalDevice,
		swapchain,
		std::numeric_limits<uint64_t>::max(),
		readyToDraw[currentFrame],
		VK_NULL_HANDLE,
		&imageIndex);

	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &readyToDraw[currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &readyToPresent[currentFrame];

	result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to submit command buffer");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &readyToPresent[currentFrame];
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = &result;

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to present the image to the screen");

	currentFrame = (currentFrame < (MAX_FRAME_COUNT-1)) ? ++currentFrame : 0;

}

void VulkanRenderer::cleanup() noexcept
{
	vkDeviceWaitIdle(mainDevice.logicalDevice);


	for(auto& mesh : meshes)
		mesh.destroyVertexBuffer();

	for (size_t i = 0; i < MAX_FRAME_COUNT; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, readyToPresent[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, readyToDraw[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);

	for (auto& framebuffer : swapChainFramebuffers)
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);

	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

	for (auto& image : swapChainImages)
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);

	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (enableValidationLayers)
		DestroyDebugReportCallbackEXT(instance, callback, nullptr);

	vkDestroyInstance(instance, nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
	cleanup();
}

void VulkanRenderer::createVkInstance()
{
	//Checking Validation Layers
	if(enableValidationLayers && !checkValidationLayerSupport())
		throw std::runtime_error("validation layers requested, but not available!");

	//Info about the application itself
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "Vulkcan Tutorial";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	//Creation Information to create the vulkan instance
	VkInstanceCreateInfo createInfo = {};

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.pApplicationInfo = &appInfo;
	
	//List of instance extensions
	std::vector<const char*> instanceExtensions;

	uint32_t glfwReqExtensionCount = 0;

	const char** glfwReqExtensions = 
		glfwGetRequiredInstanceExtensions(&glfwReqExtensionCount);

	//Adding the required GLFW extensions to the extension list
	instanceExtensions.insert(
		instanceExtensions.end(), 
		glfwReqExtensions, 
		glfwReqExtensions + glfwReqExtensionCount);

	if (enableValidationLayers) {
		instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	createInfo.enabledLayerCount = (enableValidationLayers) ? 
		static_cast<uint32_t>(validationLayers.size()) : 0;

	createInfo.ppEnabledLayerNames = (enableValidationLayers)? 
		validationLayers.data() : nullptr;

	bool supported = checkInstanceExtensionSupport(instanceExtensions);

	if (supported)
	{
		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to create a Vulkan Instance!");

		return;
	}
	
	throw std::runtime_error("Failed to create a Vulkan Instance!, Unsoported Instance Extensions");
}

void VulkanRenderer::createDebugCallback()
{
	// Only create callback if validation enabled
	if (!enableValidationLayers) return;

	VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
	callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | 
		VK_DEBUG_REPORT_WARNING_BIT_EXT | 
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT | 
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_DEBUG_BIT_EXT;	// Which validation reports should initiate callback
	callbackCreateInfo.pfnCallback = debugCallback;												// Pointer to callback function itself

	// Create debug callback with custom create function
	VkResult result = CreateDebugReportCallbackEXT(instance, &callbackCreateInfo, nullptr, &callback);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Debug Callback!");
	}
}

void VulkanRenderer::createLogicalDevice()
{
	//Queue Family and Queue Priority(0.0f to 1.0f)
	auto queueFamilies = getQueueFamilies(mainDevice.physicalDevice);
	float priotity = 1.0f;

	//The graphics could be the same as the presentation queue, however if 
	//there is more than one VkDeviceQueueCreateInfo on the same device with the same queue index
	//Vulkan crahes, so here I'm making sure that doesnt happen
	std::set<int> queueSet{ queueFamilies.graphicsFamily, queueFamilies.presentationFamily };

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueSet.size());

	//Create one VkDeviceQueueCreateInfo per unique family in "queueFamilies"
	for (VkDeviceQueueCreateInfo& item : queueCreateInfos)
	{
		//Device Queue Creation Info
		item = {};
		item.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		item.queueFamilyIndex = queueFamilies.graphicsFamily;
		item.queueCount = 1;
		item.pQueuePriorities = &priotity;
	}

	//Physical Device Features
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(mainDevice.physicalDevice, &deviceFeatures);
	
	//Logical Device Creation Info
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	//Create Logical Device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);

	//Logical Device Creation Validation
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Logical Device!");

	//Getting access to the Graphics queue
	vkGetDeviceQueue(mainDevice.logicalDevice, queueFamilies.graphicsFamily, 0, &graphicsQueue);

	//Getting access to the Presentation queue
	vkGetDeviceQueue(mainDevice.logicalDevice,queueFamilies.presentationFamily,0,&presentationQueue);
}

void VulkanRenderer::createSurface()
{
	//Create Surface
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr,&surface);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create a Surface!");
}

void VulkanRenderer::createSwapChain()
{
	SwapChainDetails swapChainDetails = getSwapchainDetails(mainDevice.physicalDevice);
	auto scPtr = &swapChainDetails.surfaceCapabilities;

	VkSurfaceFormatKHR format = chooseFormat(swapChainDetails.surfaceFormats);
	VkPresentModeKHR presentMode = choosePresentationMode(swapChainDetails.surfacePresentationModes);
	VkExtent2D extent = chooseSwapChainExtent(swapChainDetails.surfaceCapabilities);

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = nullptr;
	swapChainCreateInfo.flags = 0;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.imageFormat = format.format;
	swapChainCreateInfo.imageColorSpace = format.colorSpace;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.imageExtent = extent;

	//Useful when there is another swap chain that is going to be destroyed, then this new one
	//would take its responsibilities
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	swapChainCreateInfo.minImageCount = (scPtr->minImageCount == scPtr->maxImageCount)?
		scPtr->minImageCount : scPtr->minImageCount + 1;

	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.preTransform = scPtr->currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;

	auto queueIndices = getQueueFamilies(mainDevice.physicalDevice);

	if (queueIndices.graphicsFamily != queueIndices.presentationFamily)
	{
		uint32_t queueFamilies[] = { queueIndices.graphicsFamily, queueIndices.presentationFamily };

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilies;
	}
	else 
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create a SwapChain!");

	swapChainExtent = extent;
	swapChainFormat = format.format;

	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);

	std::vector<VkImage>images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

	swapChainImages.resize(swapChainImageCount);

	for (int i =0; i< swapChainImageCount; i++)
	{
		swapChainImages[i].image = images[i];
		swapChainImages[i].imageView = createImageView(images[i], swapChainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VulkanRenderer::createRenderPass()
{
	//************************** COLOR ATTACHMENT *****************************
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

	//Describes what to do with attachments before rendering VK_ATTACHMENT_LOAD_OP_CLEAR = glClear()
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	//Describes what to do with the attachment after rendering 
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	//TODO (When implementing Depth Stencil)
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	//Framebuffer data will be stored as an image, but images can be stored in different laouts
	//for them to be optimal for certain operations

	//Describes the layout of the image before entering the render pass
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	//Describes the layout of the image after the render pass 
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//****************************** CREATE SUBPASS DESCRIPTION ************************************
	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	//Describes the pipeline type to be bound to the subpass
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//Determines when to do image layout transitions
	//The first will transition from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	//The second will transition from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	VkSubpassDependency subpassDependencies[2];

	subpassDependencies[0].dependencyFlags = 0;
	subpassDependencies[1].dependencyFlags = 0;

	//Describes the previous subpass, from which image layout is going to be transformed 
	//VK_SUBPASS_EXTERNAL = outside the render pass 
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;

	//Defines after what stage of the src subpass do the image layout transition 
	//VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = once its finished
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	//Defines after what operation within the "srcStageMask" to do the image layout transition
	//VK_ACCESS_MEMORY_READ_BIT = once the memory is read
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	//Defines before what stage of the dst subpass to do the image layout transition
	//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = before the fragment shader outputs the color
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	//Defines before what operation within the "dstStageMask" to do the image layout transition
	//VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = before reading or writing to the attachment
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	//Describes the next subpass to which image layout is going to be transformed to
	//0 = the first subpass in the render pass
	subpassDependencies[0].dstSubpass = 0;

	//Just the same but in reverse order :)
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;

	//************************** RENDER PASS CREATE INFO ***************************************
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pNext = nullptr;
	renderPassCreateInfo.flags = 0;
	renderPassCreateInfo.attachmentCount = (uint32_t)1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = (uint32_t)1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = (uint32_t)2;
	renderPassCreateInfo.pDependencies = subpassDependencies;

	VkResult result = vkCreateRenderPass(
		mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass!");
}

void VulkanRenderer::createGraphicsPipeline()
{
	auto vertexCode = readFile("Shaders/vert.spv");
	auto fragmentCode = readFile("Shaders/frag.spv");

	//*************************BUILD SHADER MODULE TO LINK TO THE GRAPHICS PIPELINE***********************
	VkShaderModule vertexModule = createShaderModule(vertexCode);
	VkShaderModule fragmentModule = createShaderModule(fragmentCode);


	//*****************************CREATE VERTEX SHADER STAGE CREATE INFO*************************
	VkPipelineShaderStageCreateInfo vertexStateCreateInfo = {};
	vertexStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexStateCreateInfo.pNext = nullptr;
	vertexStateCreateInfo.module = vertexModule;
	vertexStateCreateInfo.flags = 0;
	vertexStateCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexStateCreateInfo.pName = "main";


	//*************************CREATE FRAGMENT SHADER STAGE CREATE INFO*****************************
	VkPipelineShaderStageCreateInfo fragmentStateCreateInfo = {};
	fragmentStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentStateCreateInfo.pNext = nullptr;
	fragmentStateCreateInfo.module = fragmentModule;
	fragmentStateCreateInfo.flags = 0;
	fragmentStateCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentStateCreateInfo.pName = "main";

	//Graphics pipeline requires an array with all the shaders create info
	VkPipelineShaderStageCreateInfo shaders[] = { vertexStateCreateInfo, fragmentStateCreateInfo };


	//*************************CREATE VERTEX INPUT CREATE INFO*************************************
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.pNext = nullptr;
	vertexInputCreateInfo.flags = 0;

	VkVertexInputBindingDescription bindingDescription;
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(VertexData);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// List of vertex binding descriptions(data spacing, stride etc...)
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.vertexBindingDescriptionCount = (uint32_t)1;

	std::array<VkVertexInputAttributeDescription, 2> attributeDescription;
	attributeDescription[0].binding = 0;
	attributeDescription[0].location = 0;
	attributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription[0].offset = offsetof(VertexData, position);

	attributeDescription[1].binding = 0;
	attributeDescription[1].location = 1;
	attributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription[1].offset = offsetof(VertexData, color);


	//List of vertex attribute descriptions(data format, and where to bind to)
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescription.data();
	vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescription.size();


	//**************************CREATE INPUT ASSEMBLY CREATE INFO***********************************
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.pNext = nullptr;
	inputAssemblyCreateInfo.flags = 0;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;


	//*****************************CREATE VIEWPORT & SCISSOR CREATE INFO******************************
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	
	VkRect2D scissor = {};
	scissor.offset = { 0,0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.pNext = nullptr;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.pScissors = &scissor;


	//****************************CREATE RASTERIZER CREATE INFO****************************************
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.pNext = nullptr;
	rasterizerCreateInfo.flags = 0;

	//Change if fragments beyond the far plane are clipped
	rasterizerCreateInfo.depthClampEnable = VK_FALSE; 

	//Whether to discard the data and skip the rasterization phase
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE; 

	//Defines how fragments will be created from the input assembly data
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;

	rasterizerCreateInfo.lineWidth = 1;

	//Tells which part of the primitive to draw 'VK_CULL_MODE_BACK_BIT' = just draw the front
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	//Determines which side is front
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

	//Wether to add depth bias to fragments (good for stopping "shadow acne")
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;


	//****************************MULTISAMPLING CREATE INFO****************************************
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;


	//***************************COLOR BLENDING CREATE INFO*********************************************
	VkPipelineColorBlendAttachmentState colorState = {};

	//Tells which colors to apply blending to
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |VK_COLOR_COMPONENT_A_BIT;

	colorState.blendEnable = VK_TRUE;

	//Blending formula (srcAlphaBlendFactor * color1) colorBlendOp (dstColorBlendFactor * color2)
	//It will be the (VK_BLEND_FACTOR_SRC_ALPHA * color1) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * color2)
	//which is the same as (color1.alpha * color1) + ((1-color1.alpha)*color2) which is a basic linear interpolation
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;

	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo blendCreateInfo = {};
	blendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendCreateInfo.logicOpEnable = VK_FALSE;
	blendCreateInfo.attachmentCount = 1;
	blendCreateInfo.pAttachments = &colorState;


	//*******************************PIPELINE LAYOUT*****************************************
	//For descriptor sets and push constants (what in OpenGL are Uniforms)
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(
		mainDevice.logicalDevice, 
		&pipelineLayoutCreateInfo, 
		nullptr, 
		&pipelineLayout);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipeline loayout!");


	//************************CREATE GRAPHICS PIPELINE CREATE INFO*********************************
	VkGraphicsPipelineCreateInfo gPipelineCreateInfo = {};
	gPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gPipelineCreateInfo.pNext = nullptr;
	gPipelineCreateInfo.flags = 0;
	gPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	gPipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	gPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	gPipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	gPipelineCreateInfo.pViewportState = &viewportCreateInfo;
	gPipelineCreateInfo.pDynamicState = nullptr;
	gPipelineCreateInfo.pColorBlendState = &blendCreateInfo;
	gPipelineCreateInfo.pDepthStencilState = nullptr;
	gPipelineCreateInfo.stageCount = 2;
	gPipelineCreateInfo.pStages = shaders;
	gPipelineCreateInfo.layout = pipelineLayout;
	gPipelineCreateInfo.renderPass = renderPass;
	gPipelineCreateInfo.subpass = 0;
	gPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	gPipelineCreateInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(
		mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &gPipelineCreateInfo, nullptr, &graphicsPipeline);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create the graphics pipeline!");

	//***************************DESTROY SHADER MODULES********************************************
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexModule, nullptr);
}

void VulkanRenderer::createFramebuffers()
{
	swapChainFramebuffers.resize(swapChainImages.size());

	for (int i = 0; i < swapChainImages.size(); i++)
	{
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		
		VkImageView attachments[] = { swapChainImages[i].imageView };

		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = nullptr;
		frameBufferCreateInfo.flags = 0;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.attachmentCount = 1;
		frameBufferCreateInfo.pAttachments = attachments;
		frameBufferCreateInfo.width = swapChainExtent.width;
		frameBufferCreateInfo.height = swapChainExtent.height;
		frameBufferCreateInfo.layers = (uint32_t)1;

		VkResult result = vkCreateFramebuffer(
			mainDevice.logicalDevice, 
			&frameBufferCreateInfo, 
			nullptr, 
			&swapChainFramebuffers[i]);

		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to create the framebuffer!");
	}
}

void VulkanRenderer::createCommandPool()
{
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};

	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;

	commandPoolCreateInfo.queueFamilyIndex = 
		getQueueFamilies(mainDevice.physicalDevice).graphicsFamily;

	VkResult result = vkCreateCommandPool(
		mainDevice.logicalDevice, &commandPoolCreateInfo, nullptr, &graphicsCommandPool);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create the graphics command pool!");

}

void VulkanRenderer::createCommandBuffers()
{
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo commandBufferAllocInfo = {};

	commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocInfo.pNext = nullptr;
	commandBufferAllocInfo.commandPool = graphicsCommandPool;
	commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(swapChainFramebuffers.size()); 

	VkResult result  = vkAllocateCommandBuffers(
		mainDevice.logicalDevice, &commandBufferAllocInfo, commandBuffers.data());

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffers on the command pool!");
}

void VulkanRenderer::createSyncronization()
{
	readyToDraw.resize(MAX_FRAME_COUNT);
	readyToPresent.resize(MAX_FRAME_COUNT);
	drawFences.resize(MAX_FRAME_COUNT);

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for(int i =0; i<MAX_FRAME_COUNT; i++)
		if(vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &readyToDraw[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &readyToPresent[i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create the syncronization mechanism!");
}

void VulkanRenderer::recordCommands()
{
	VkClearValue clearValues[] = { 
		VkClearValue{0.6f, 0.65f, 0.4, 1.0f}
	};

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset = { 0,0 };
	renderPassBeginInfo.renderArea.extent = swapChainExtent;
	renderPassBeginInfo.pClearValues = clearValues;
	renderPassBeginInfo.clearValueCount = (uint32_t)1;

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		VkResult result = vkBeginCommandBuffer(commandBuffers[i], &beginInfo);
		
		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to start recording command buffers!");

		std::vector<VkBuffer> vertexBuffers(meshes.size());
		std::vector<VkDeviceSize> offsets(meshes.size(),0);

		int vertexCount = 0;

		for (int j = 0; j < meshes.size(); j++)
		{
			vertexBuffers[j] = meshes[j].getVertexBuffer();
			vertexCount += meshes[j].getVerticesCount();
		}
					
		renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];

		//RECORDING COMMANDS
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
			
			vkCmdBindVertexBuffers(commandBuffers[i], 0, vertexBuffers.size(), vertexBuffers.data(), offsets.data());

			vkCmdDraw(commandBuffers[i], vertexCount, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);


		result = vkEndCommandBuffer(commandBuffers[i]);

		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to end recording command buffers!");
	}
}

bool VulkanRenderer::checkInstanceExtensionSupport(const std::vector<const char*>& extensions)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties>supportedExtensions(extensionCount);

	vkEnumerateInstanceExtensionProperties(
		nullptr, &extensionCount, supportedExtensions.data());

	for (const auto& extension : extensions)
	{
		auto element = std::find_if(supportedExtensions.begin(), supportedExtensions.end(),
			[&](auto vkEp){ return strcmp(vkEp.extensionName, extension) == 0;});

		if (element == supportedExtensions.end())
			return false;
	}

	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	//In order to a Device being suitable we need:
	
	//1 support the required device extensions:
	bool supportDeviceExt = checkDeviceExtensions(device);

	//2:Suport the swapchain
	SwapChainDetails scDetails = getSwapchainDetails(device);

	//3: Support the required queue families
	QueueFamilyIndices queueFamilies = getQueueFamilies(device);
	
	return queueFamilies.isValid() && supportDeviceExt && scDetails.isValid();
}

bool VulkanRenderer::checkDeviceExtensions(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0)
		return false;

	std::vector<VkExtensionProperties> supportedExt(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, supportedExt.data());

	for (const auto& ext : deviceExtensions)
	{
		auto it = std::find_if(supportedExt.begin(), supportedExt.end(),
			[&ext](VkExtensionProperties prop) {return strcmp(prop.extensionName, ext) == 0; });

		if (it == supportedExt.end())
			return false;
	}

	return true;

}

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount,nullptr);

	std::vector<VkLayerProperties> layerProperties(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

	for (const auto& layer : validationLayers)
	{
		auto it = std::find_if(layerProperties.begin(), layerProperties.end(),
			[&layer](VkLayerProperties prop) {return strcmp(prop.layerName, layer) == 0; });

		if (it == layerProperties.end())
			return false;
	}

	return true;	
}

void VulkanRenderer::getPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
		throw std::runtime_error("Cannot find a Vulkan compatible GPU");

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

	auto it = std::find_if(physicalDevices.begin(), physicalDevices.end(),
		[&](auto device) {return checkDeviceSuitable(device); });

	if (it == physicalDevices.end())
		throw std::runtime_error("No Graphics queue family avaliable");

	mainDevice.physicalDevice = *it;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices queueIndices = {};
	VkBool32 presentationSupport = false;

	bool foundGraphicsQueue, foundPresentationQueue = foundGraphicsQueue = false;

	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);

	std::vector<VkQueueFamilyProperties> queues(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queues.data());


	for (auto it = queues.begin(); it < queues.end(); it++)
	{
		if (foundGraphicsQueue && foundPresentationQueue)
			break;

		int index = it - queues.begin();

		vkGetPhysicalDeviceSurfaceSupportKHR(
			device,
			index, 
			surface,
			&presentationSupport);

		if (it->queueCount > 0 && it->queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			queueIndices.graphicsFamily = index;
			foundGraphicsQueue = true;
		}

		if (presentationSupport)
		{
			queueIndices.presentationFamily = index;
			foundPresentationQueue = true;
		}
			
	}

	return queueIndices;

}

SwapChainDetails VulkanRenderer::getSwapchainDetails(VkPhysicalDevice device)
{
	uint32_t formatsCount = 0;
	uint32_t pModesCount = 0;

	SwapChainDetails details = {};
	
	//Surface Capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		device,
		surface, 
		&details.surfaceCapabilities);


	//Surface Formats
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatsCount, nullptr);

	if (formatsCount > 0) 
		details.surfaceFormats.resize(formatsCount);

	vkGetPhysicalDeviceSurfaceFormatsKHR(
		device,
		surface, 
		&formatsCount, 
		details.surfaceFormats.data());


	//Surface Presentation Modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(device,surface, &pModesCount, nullptr);

	if (pModesCount > 0)
		details.surfacePresentationModes.resize(pModesCount);

	vkGetPhysicalDeviceSurfacePresentModesKHR(
		device,
		surface,
		&pModesCount,
		details.surfacePresentationModes.data());

	return details;
} 

VkSurfaceFormatKHR VulkanRenderer::chooseFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
{
	if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

	auto it = std::find_if(surfaceFormats.begin(), surfaceFormats.end(),
		[](VkSurfaceFormatKHR format)
		{
			return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && format.format == VK_FORMAT_R8G8B8A8_UNORM;
		});

	return (it < surfaceFormats.end()) ? *it : surfaceFormats[0];
}

VkPresentModeKHR VulkanRenderer::choosePresentationMode(const std::vector<VkPresentModeKHR>& surfacePresentationModes)
{
	auto it = std::find_if(surfacePresentationModes.begin(), surfacePresentationModes.end(),
		[](VkPresentModeKHR presentModes) { return presentModes == VK_PRESENT_MODE_MAILBOX_KHR; });
	
	return (it < surfacePresentationModes.end()) ? *it : VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	if (surfaceCapabilities.currentExtent.height != std::numeric_limits<uint32_t>::max())
		return surfaceCapabilities.currentExtent;

	VkExtent2D windowExtent = {};
	int width, height;

	glfwGetFramebufferSize(window, &width, &height);
		
	windowExtent.height = std::max(
		std::min((int)surfaceCapabilities.maxImageExtent.width, width),
		(int)surfaceCapabilities.minImageExtent.width);

	windowExtent.height = std::max(
		std::min((int)surfaceCapabilities.maxImageExtent.height, height),
		(int)surfaceCapabilities.minImageExtent.height);

	return windowExtent;	
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags)
{
	VkImageView imageView = {};
	VkImageViewCreateInfo imageViewCreateInfo = {};

	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;
	imageViewCreateInfo.flags = 0;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

	//Allows remaping the rgba channles to ther rgba values
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	//Sets what aspect of the image to view (COLOR_BIT, DEPTH_BIT, etc...)
	imageViewCreateInfo.subresourceRange.aspectMask = flags;

	//Sets the start mipmap to look at by the image
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;

	//Sets the total number of mipmaps to look at
	imageViewCreateInfo.subresourceRange.levelCount = 1;

	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &imageViewCreateInfo, nullptr, &imageView);

	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create a ImageView!");
	
	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	VkShaderModule shaderModule = {};
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};

	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pNext = nullptr;
	shaderModuleCreateInfo.flags = 0;
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
		std::runtime_error("Failed to create a shader module!");

	return shaderModule;
}
