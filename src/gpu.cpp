#include "gpu.h"

#include "debug.h"
#include "heap.h"
#include "wm.h"

#include "gui_helper.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#include <malloc.h>
#include <string.h>

typedef struct gpu_cmd_buffer_t
{
	VkCommandBuffer buffer;
	VkPipelineLayout pipeline_layout;
	int index_count;
	int vertex_count;
} gpu_cmd_buffer_t;
 
typedef struct gpu_descriptor_t
{
	VkDescriptorSet set;
} gpu_descriptor_t;

typedef struct gpu_mesh_t
{
	VkBuffer index_buffer;
	VkDeviceMemory index_memory;
	int index_count;
	VkIndexType index_type;

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_memory;
	int vertex_count;
} gpu_mesh_t;

typedef struct gpu_pipeline_t
{
	VkPipelineLayout pipeline_layout;
	VkPipeline pipe;
} gpu_pipeline_t;

typedef struct gpu_shader_t
{
	VkShaderModule vertex_module;
	VkShaderModule fragment_module;
	VkDescriptorSetLayout descriptor_set_layout;
} gpu_shader_t;

typedef struct gpu_uniform_buffer_t
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDescriptorBufferInfo descriptor;
} gpu_uniform_buffer_t;

typedef struct gpu_frame_t
{
	VkImage image;
	VkImageView view;
	VkFramebuffer frame_buffer;
	VkFence fence;
	gpu_cmd_buffer_t* cmd_buffer;
} gpu_frame_t;

typedef struct gpu_t
{
	heap_t* heap;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkQueue queue;
	uint32_t queue_family;
	VkSurfaceKHR surface;
	VkSwapchainKHR swap_chain;

	VkRenderPass render_pass;

	VkImage depth_stencil_image;
	VkDeviceMemory depth_stencil_memory;
	VkImageView depth_stencil_view;

	VkCommandPool cmd_pool;
	VkDescriptorPool descriptor_pool;

	VkSemaphore present_complete_sema;
	VkSemaphore render_complete_sema;

	VkPipelineInputAssemblyStateCreateInfo mesh_input_assembly_info[k_gpu_mesh_layout_count];
	VkPipelineVertexInputStateCreateInfo mesh_vertex_input_info[k_gpu_mesh_layout_count];
	VkIndexType mesh_vertex_size[k_gpu_mesh_layout_count];
	VkIndexType mesh_index_size[k_gpu_mesh_layout_count];
	VkIndexType mesh_index_type[k_gpu_mesh_layout_count];

	uint32_t frame_width;
	uint32_t frame_height;

	gpu_frame_t* frames;
	uint32_t frame_count;
	uint32_t frame_index;
} gpu_t;

static void create_mesh_layouts(gpu_t* gpu);
static void destroy_mesh_layouts(gpu_t* gpu);
static uint32_t get_memory_type_index(gpu_t* gpu, uint32_t bits, VkMemoryPropertyFlags properties);

gpu_t* gpu_create(heap_t* heap, wm_window_t* window)
{
	gpu_t* gpu = (gpu_t*) heap_alloc(heap, sizeof(gpu_t), 8);
	memset(gpu, 0, sizeof(*gpu));
	gpu->heap = heap;

	//////////////////////////////////////////////////////
	// Create VkInstance
	//////////////////////////////////////////////////////

	bool use_validation = GetEnvironmentVariableW(L"VK_LAYER_PATH", NULL, 0) > 0;

	VkApplicationInfo app_info;

	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "GA 2022";
	app_info.pEngineName = "GA 2022";
	app_info.apiVersion = VK_API_VERSION_1_2;

	const char* k_extensions[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	};

	const char* k_layers[] =
	{
		"VK_LAYER_KHRONOS_validation",
	};

	VkInstanceCreateInfo instance_info;
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pApplicationInfo = &app_info;
	instance_info.enabledExtensionCount = _countof(k_extensions);
	instance_info.ppEnabledExtensionNames = k_extensions;
	instance_info.enabledLayerCount = use_validation ? _countof(k_layers) : 0;
	instance_info.ppEnabledLayerNames = k_layers;
	

	const char* function = NULL;
	VkResult result = vkCreateInstance(&instance_info, NULL, &gpu->instance);
	if (result)
	{
		function = "vkCreateInstance";
		//goto fail;
	}

	//////////////////////////////////////////////////////
	// Find our desired VkPhysicalDevice (GPU)
	//////////////////////////////////////////////////////

	uint32_t physical_device_count = 0;
	result = vkEnumeratePhysicalDevices(gpu->instance, &physical_device_count, NULL);
	if (result)
	{
		function = "vkEnumeratePhysicalDevices";
		//goto fail;
	}

	if (!physical_device_count)
	{
		debug_print(k_print_error, "No device with Vulkan support found!\n");
		gpu_destroy(gpu);
		return NULL;
	}

	VkPhysicalDevice* physical_devices = (VkPhysicalDevice*) alloca(sizeof(VkPhysicalDevice) * physical_device_count);
	result = vkEnumeratePhysicalDevices(gpu->instance, &physical_device_count, physical_devices);
	if (result)
	{
		function = "vkEnumeratePhysicalDevices";
		//goto fail;
	}

	gpu->physical_device = physical_devices[0];

	//////////////////////////////////////////////////////
	// Create a VkDevice with graphics queue
	//////////////////////////////////////////////////////

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical_device, &queue_family_count, NULL);

	VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*) alloca(sizeof(VkQueueFamilyProperties) * queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical_device, &queue_family_count, queue_families);

	uint32_t queue_family_index = UINT32_MAX;
	uint32_t queue_count = UINT32_MAX;
	for (uint32_t i = 0; i < queue_family_count; ++i)
	{
		if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			queue_family_index = i;
			queue_count = queue_families[i].queueCount;
			break;
		}
	}
	if (queue_count == UINT32_MAX)
	{
		debug_print(k_print_error, "No device with graphics queue found!\n");
		gpu_destroy(gpu);
		return NULL;
	}

	float* queue_priorites = (float*) alloca(sizeof(float) * queue_count);
	memset(queue_priorites, 0, sizeof(float) * queue_count);

	VkDeviceQueueCreateInfo queue_info;
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.queueFamilyIndex = queue_family_index;
	queue_info.queueCount = queue_count;
	queue_info.pQueuePriorities = queue_priorites;

	const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo device_info;
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos = &queue_info;
	device_info.enabledExtensionCount = _countof(device_extensions);
	device_info.ppEnabledExtensionNames = device_extensions;

	result = vkCreateDevice(gpu->physical_device, &device_info, NULL, &gpu->logical_device);
	if (result)
	{
		function = "vkCreateDevice";
		//goto fail;
	}

	vkGetPhysicalDeviceMemoryProperties(gpu->physical_device, &gpu->memory_properties);
	vkGetDeviceQueue(gpu->logical_device, queue_family_index, 0, &gpu->queue);
	gpu->queue_family = queue_family_index;

	//////////////////////////////////////////////////////
	// Create a Windows surface on which to render
	//////////////////////////////////////////////////////

	VkWin32SurfaceCreateInfoKHR surface_info;
	surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_info.hinstance = GetModuleHandle(NULL);
	surface_info.hwnd = (HWND) wm_get_raw_window(window);

	result = vkCreateWin32SurfaceKHR(gpu->instance, &surface_info, NULL, &gpu->surface);
	if (result)
	{
		function = "vkCreateWin32SurfaceKHR";
		//goto fail;
	}

	VkSurfaceCapabilitiesKHR surface_cap;
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->physical_device, gpu->surface, &surface_cap);
	if (result)
	{
		function = "vkGetPhysicalDeviceSurfaceCapabilitiesKHR";
		//goto fail;
	}

	gpu->frame_width = surface_cap.currentExtent.width;
	gpu->frame_height = surface_cap.currentExtent.height;

	//////////////////////////////////////////////////////
	// Create a VkSwapchain storing frame buffer images
	//////////////////////////////////////////////////////

	VkSwapchainCreateInfoKHR swapchain_info;
	swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_info.surface = gpu->surface;
	swapchain_info.minImageCount = __max(surface_cap.minImageCount + 1, 3);
	swapchain_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
	swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchain_info.imageExtent = surface_cap.currentExtent;
	swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_info.preTransform = surface_cap.currentTransform;
	swapchain_info.imageArrayLayers = 1;
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchain_info.clipped = VK_TRUE;
	swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	
	result = vkCreateSwapchainKHR(gpu->logical_device, &swapchain_info, NULL, &gpu->swap_chain);
	if (result)
	{
		function = "vkCreateSwapchainKHR";
		//goto fail;
	}

	result = vkGetSwapchainImagesKHR(gpu->logical_device, gpu->swap_chain, &gpu->frame_count, NULL);
	if (result)
	{
		function = "vkGetSwapchainImagesKHR";
		//goto fail;
	}

	gpu->frames = (gpu_frame_t*) heap_alloc(heap, sizeof(gpu_frame_t) * gpu->frame_count, 8);
	memset(gpu->frames, 0, sizeof(gpu_frame_t) * gpu->frame_count);
	VkImage* images = (VkImage*) alloca(sizeof(VkImage) * gpu->frame_count);

	result = vkGetSwapchainImagesKHR(gpu->logical_device, gpu->swap_chain, &gpu->frame_count, images);
	if (result)
	{
		function = "vkGetSwapchainImagesKHR";
		//goto fail;
	}

	for (uint32_t i = 0; i < gpu->frame_count; i++)
	{
		gpu->frames[i].image = (VkImage) images[i];

		VkImageViewCreateInfo image_view_info;
		image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_info.format = VK_FORMAT_B8G8R8A8_SRGB;
		image_view_info.components =
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_view_info.subresourceRange.levelCount = 1;
		image_view_info.subresourceRange.layerCount = 1;
		image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		image_view_info.image = images[i];

		result = vkCreateImageView(gpu->logical_device, &image_view_info, NULL, &gpu->frames[i].view);
		if (result)
		{
			function = "vkCreateImageView";
			//goto fail;
		}
	}

	//////////////////////////////////////////////////////
	// Create a depth buffer image
	//////////////////////////////////////////////////////
	{
		VkImageCreateInfo depth_image_info;
		
		depth_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		depth_image_info.imageType = VK_IMAGE_TYPE_2D;
		depth_image_info.format = VK_FORMAT_D32_SFLOAT;
		depth_image_info.extent = { surface_cap.currentExtent.width, surface_cap.currentExtent.height, 1 };
		depth_image_info.mipLevels = 1;
		depth_image_info.arrayLayers = 1;
		depth_image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		depth_image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depth_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		
		result = vkCreateImage(gpu->logical_device, &depth_image_info, NULL, &gpu->depth_stencil_image);
		if (result)
		{
			function = "vkCreateImage";
			//goto fail;
		}

		VkMemoryRequirements depth_mem_reqs;
		vkGetImageMemoryRequirements(gpu->logical_device, gpu->depth_stencil_image, &depth_mem_reqs);

		VkMemoryAllocateInfo depth_alloc_info;
		depth_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		depth_alloc_info.allocationSize = depth_mem_reqs.size;
		depth_alloc_info.memoryTypeIndex = get_memory_type_index(gpu, depth_mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		result = vkAllocateMemory(gpu->logical_device, &depth_alloc_info, NULL, &gpu->depth_stencil_memory);
		if (result)
		{
			function = "vkAllocateMemory";
			//goto fail;
		}

		result = vkBindImageMemory(gpu->logical_device, gpu->depth_stencil_image, gpu->depth_stencil_memory, 0);
		if (result)
		{
			function = "vkBindImageMemory";
			//goto fail;
		}

		VkImageViewCreateInfo depth_view_info;
		depth_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		depth_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depth_view_info.format = VK_FORMAT_D32_SFLOAT;
		depth_view_info.subresourceRange; 
		depth_view_info.subresourceRange.levelCount = 1;
		depth_view_info.subresourceRange.layerCount = 1;
		depth_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		depth_view_info.image = gpu->depth_stencil_image;
		
		result = vkCreateImageView(gpu->logical_device, &depth_view_info, NULL, &gpu->depth_stencil_view);
		if (result)
		{
			function = "vkCreateImageView";
			//goto fail;
		}
	}

	//////////////////////////////////////////////////////
	// Create a VkRenderPass that draws to the screen
	//////////////////////////////////////////////////////
	{
		VkAttachmentDescription attachments[2];
		
		attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,		
	
		attachments[1].format = VK_FORMAT_D32_SFLOAT;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


		VkAttachmentReference color_reference;
		color_reference.attachment = 0;
		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		VkAttachmentReference depth_reference;
		depth_reference.attachment = 1;
		depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_reference;
		subpass.pDepthStencilAttachment = &depth_reference;

		VkSubpassDependency dependencies[2];

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = 0;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		
		VkRenderPassCreateInfo render_pass_info;
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = _countof(attachments);
		render_pass_info.pAttachments = attachments;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = _countof(dependencies);
		render_pass_info.pDependencies = dependencies;
		
		result = vkCreateRenderPass(gpu->logical_device, &render_pass_info, NULL, &gpu->render_pass);
		if (result)
		{
			function = "vkCreateRenderPass";
			//goto fail;
		}
	}

	//////////////////////////////////////////////////////
	// Create VkFramebuffer objects
	//////////////////////////////////////////////////////
	for (uint32_t i = 0; i < gpu->frame_count; i++)
	{
		VkImageView attachments[2] = { gpu->frames[i].view, gpu->depth_stencil_view };

		VkFramebufferCreateInfo frame_buffer_info;
		frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buffer_info.renderPass = gpu->render_pass;
		frame_buffer_info.attachmentCount = _countof(attachments);
		frame_buffer_info.pAttachments = attachments;
		frame_buffer_info.width = surface_cap.currentExtent.width;
		frame_buffer_info.height = surface_cap.currentExtent.height;
		frame_buffer_info.layers = 1;

		result = vkCreateFramebuffer(gpu->logical_device, &frame_buffer_info, NULL, &gpu->frames[i].frame_buffer);
		if (result)
		{
			function = "vkCreateFramebuffer";
			//goto fail;
		}
	}

	//////////////////////////////////////////////////////
	// Create a VkSemaphores for GPU/CPU sychronization
	//////////////////////////////////////////////////////
	VkSemaphoreCreateInfo semaphore_info;
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	result = vkCreateSemaphore(gpu->logical_device, &semaphore_info, NULL, &gpu->present_complete_sema);
	if (result)
	{
		function = "vkCreateSemaphore";
		//goto fail;
	}
	result = vkCreateSemaphore(gpu->logical_device, &semaphore_info, NULL, &gpu->render_complete_sema);
	if (result)
	{
		function = "vkCreateSemaphore";
		//goto fail;
	}

	//////////////////////////////////////////////////////
	// Create a VkDescriptorPool for use during the frame
	//////////////////////////////////////////////////////
	VkDescriptorPoolSize descriptor_pool_sizes[1];
	
	descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_pool_sizes[0].descriptorCount = 512;

	VkDescriptorPoolCreateInfo descriptor_pool_info;
	descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptor_pool_info.poolSizeCount = _countof(descriptor_pool_sizes);
	descriptor_pool_info.pPoolSizes = descriptor_pool_sizes;
	descriptor_pool_info.maxSets = 512;

	result = vkCreateDescriptorPool(gpu->logical_device, &descriptor_pool_info, NULL, &gpu->descriptor_pool);
	if (result)
	{
		function = "vkCreateDescriptorPool";
		//goto fail;
	}

	//////////////////////////////////////////////////////
	// Create a VkCommandPool for use during the frame
	//////////////////////////////////////////////////////
	VkCommandPoolCreateInfo cmd_pool_info;
	cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_pool_info.queueFamilyIndex = queue_family_index;
	cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	
	result = vkCreateCommandPool(gpu->logical_device, &cmd_pool_info, NULL, &gpu->cmd_pool);
	if (result)
	{
		function = "vkCreateCommandPool";
		goto fail;
	}

	//////////////////////////////////////////////////////
	// Create VkCommandBuffer objects for each frame
	//////////////////////////////////////////////////////
	for (uint32_t i = 0; i < gpu->frame_count; i++)
	{
		gpu->frames[i].cmd_buffer = (gpu_cmd_buffer_t*) heap_alloc(gpu->heap, sizeof(gpu_cmd_buffer_t), 8);
		memset(gpu->frames[i].cmd_buffer, 0, sizeof(gpu_cmd_buffer_t));

		VkCommandBufferAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = gpu->cmd_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		
		result = vkAllocateCommandBuffers(gpu->logical_device, &alloc_info, &gpu->frames[i].cmd_buffer->buffer);
		if (result)
		{
			function = "vkAllocateCommandBuffers";
			goto fail;
		}

		VkFenceCreateInfo fence_info;
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		result = vkCreateFence(gpu->logical_device, &fence_info, NULL, &gpu->frames[i].fence);
		if (result)
		{
			function = "vkCreateFence";
			goto fail;
		}
	}

	create_mesh_layouts(gpu);

	return gpu;

fail:
	debug_print(k_print_error, "%s failed: %d\n", function, result);
	gpu_destroy(gpu);
	return NULL;
}

void gpu_destroy(gpu_t* gpu)
{
	if (gpu && gpu->queue)
	{
		vkQueueWaitIdle(gpu->queue);
	}

	if (gpu)
	{
		destroy_mesh_layouts(gpu);
	}
	if (gpu && gpu->render_complete_sema)
	{
		vkDestroySemaphore(gpu->logical_device, gpu->render_complete_sema, NULL);
	}
	if (gpu && gpu->present_complete_sema)
	{
		vkDestroySemaphore(gpu->logical_device, gpu->present_complete_sema, NULL);
	}
	if (gpu && gpu->depth_stencil_view)
	{
		vkDestroyImageView(gpu->logical_device, gpu->depth_stencil_view, NULL);
	}
	if (gpu && gpu->depth_stencil_image)
	{
		vkDestroyImage(gpu->logical_device, gpu->depth_stencil_image, NULL);
	}
	if (gpu && gpu->depth_stencil_memory)
	{
		vkFreeMemory(gpu->logical_device, gpu->depth_stencil_memory, NULL);
	}
	if (gpu && gpu->frames)
	{
		for (uint32_t i = 0; i < gpu->frame_count; i++)
		{
			if (gpu->frames[i].fence)
			{
				vkDestroyFence(gpu->logical_device, gpu->frames[i].fence, NULL);
			}
			if (gpu->frames[i].cmd_buffer)
			{
				vkFreeCommandBuffers(gpu->logical_device, gpu->cmd_pool, 1, &gpu->frames[i].cmd_buffer->buffer);
			}
			if (gpu->frames[i].frame_buffer)
			{
				vkDestroyFramebuffer(gpu->logical_device, gpu->frames[i].frame_buffer, NULL);
			}
			if (gpu->frames[i].view)
			{
				vkDestroyImageView(gpu->logical_device, gpu->frames[i].view, NULL);
			}
		}
		heap_free(gpu->heap, gpu->frames);
	}
	if (gpu && gpu->descriptor_pool)
	{
		vkDestroyDescriptorPool(gpu->logical_device, gpu->descriptor_pool, NULL);
	}
	if (gpu && gpu->cmd_pool)
	{
		vkDestroyCommandPool(gpu->logical_device, gpu->cmd_pool, NULL);
	}
	if (gpu && gpu->render_pass)
	{
		vkDestroyRenderPass(gpu->logical_device, gpu->render_pass, NULL);
	}
	if (gpu && gpu->swap_chain)
	{
		vkDestroySwapchainKHR(gpu->logical_device, gpu->swap_chain, NULL);
	}
	if (gpu && gpu->surface)
	{
		vkDestroySurfaceKHR(gpu->instance, gpu->surface, NULL);
	}
	if (gpu && gpu->logical_device)
	{
		vkDestroyDevice(gpu->logical_device, NULL);
	}
	if (gpu && gpu->instance)
	{
		vkDestroyInstance(gpu->instance, NULL);
	}
	if (gpu)
	{
		heap_free(gpu->heap, gpu);
	}
}

int gpu_get_frame_count(gpu_t* gpu)
{
	return gpu->frame_count;
}

void gpu_wait_until_idle(gpu_t* gpu)
{
	vkQueueWaitIdle(gpu->queue);
}

gpu_descriptor_t* gpu_descriptor_create(gpu_t* gpu, const gpu_descriptor_info_t* info)
{
	gpu_descriptor_t* descriptor = (gpu_descriptor_t*) heap_alloc(gpu->heap, sizeof(gpu_descriptor_t), 8);
	memset(descriptor, 0, sizeof(*descriptor));

	VkDescriptorSetAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = gpu->descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &info->shader->descriptor_set_layout;
	
	VkResult result = vkAllocateDescriptorSets(gpu->logical_device, &alloc_info, &descriptor->set);
	if (result)
	{
		debug_print(k_print_error, "vkAllocateDescriptorSets failed: %d\n", result);
		gpu_descriptor_destroy(gpu, descriptor);
		return NULL;
	}

	VkWriteDescriptorSet* write_sets = (VkWriteDescriptorSet*) alloca(sizeof(VkWriteDescriptorSet) * info->uniform_buffer_count);
	for (int i = 0; i < info->uniform_buffer_count; ++i)
	{
		//write_sets[i] = (VkWriteDescriptorSet);
		write_sets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_sets[i].dstSet = descriptor->set;
		write_sets[i].descriptorCount = 1;
		write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write_sets[i].pBufferInfo = &info->uniform_buffers[i]->descriptor;
		write_sets[i].dstBinding = i;
	}
	vkUpdateDescriptorSets(gpu->logical_device, info->uniform_buffer_count, write_sets, 0, NULL);

	return descriptor;
}

void gpu_descriptor_destroy(gpu_t* gpu, gpu_descriptor_t* descriptor)
{
	if (descriptor && descriptor->set)
	{
		vkFreeDescriptorSets(gpu->logical_device, gpu->descriptor_pool, 1, &descriptor->set);
	}
	if (descriptor)
	{
		heap_free(gpu->heap, descriptor);
	}
}

gpu_mesh_t* gpu_mesh_create(gpu_t* gpu, const gpu_mesh_info_t* info)
{
	gpu_mesh_t* mesh = (gpu_mesh_t*) heap_alloc(gpu->heap, sizeof(gpu_mesh_t), 8);
	memset(mesh, 0, sizeof(*mesh));

	mesh->index_type = gpu->mesh_index_type[info->layout];
	mesh->index_count = (int)info->index_data_size / gpu->mesh_index_size[info->layout];
	mesh->vertex_count = (int)info->vertex_data_size / gpu->mesh_vertex_size[info->layout];

	// Vertex data
	{
		VkBufferCreateInfo vertex_buffer_info;
		vertex_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertex_buffer_info.size = info->vertex_data_size;
		vertex_buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		
		VkResult result = vkCreateBuffer(gpu->logical_device, &vertex_buffer_info, NULL, &mesh->vertex_buffer);
		if (result)
		{
			debug_print(k_print_error, "vkCreateBuffer failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}

		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(gpu->logical_device, mesh->vertex_buffer, &mem_reqs);

		VkMemoryAllocateInfo mem_alloc;
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.allocationSize = mem_reqs.size;
		mem_alloc.memoryTypeIndex = get_memory_type_index(gpu, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		result = vkAllocateMemory(gpu->logical_device, &mem_alloc, NULL, &mesh->vertex_memory);
		if (result)
		{
			debug_print(k_print_error, "vkAllocateMemory failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}

		void* vertex_dest = NULL;
		result = vkMapMemory(gpu->logical_device, mesh->vertex_memory, 0, mem_alloc.allocationSize, 0, &vertex_dest);
		if (result)
		{
			debug_print(k_print_error, "vkMapMemory failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}
		memcpy(vertex_dest, info->vertex_data, info->vertex_data_size);
		vkUnmapMemory(gpu->logical_device, mesh->vertex_memory);

		result = vkBindBufferMemory(gpu->logical_device, mesh->vertex_buffer, mesh->vertex_memory, 0);
		if (result)
		{
			debug_print(k_print_error, "vkBindBufferMemory failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}
	}

	// Index data
	{
		VkBufferCreateInfo index_buffer_info;
		index_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		index_buffer_info.size = info->index_data_size;
		index_buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		VkResult result = vkCreateBuffer(gpu->logical_device, &index_buffer_info, NULL, &mesh->index_buffer);
		if (result)
		{
			debug_print(k_print_error, "vkCreateBuffer failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}

		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(gpu->logical_device, mesh->index_buffer, &mem_reqs);

		VkMemoryAllocateInfo mem_alloc;
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.allocationSize = mem_reqs.size;
		mem_alloc.memoryTypeIndex = get_memory_type_index(gpu, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		result = vkAllocateMemory(gpu->logical_device, &mem_alloc, NULL, &mesh->index_memory);
		if (result)
		{
			debug_print(k_print_error, "vkAllocateMemory failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}

		void* index_dest = NULL;
		result = vkMapMemory(gpu->logical_device, mesh->index_memory, 0, mem_alloc.allocationSize, 0, &index_dest);
		if (result)
		{
			debug_print(k_print_error, "vkMapMemory failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}
		memcpy(index_dest, info->index_data, info->index_data_size);
		vkUnmapMemory(gpu->logical_device, mesh->index_memory);

		result = vkBindBufferMemory(gpu->logical_device, mesh->index_buffer, mesh->index_memory, 0);
		if (result)
		{
			debug_print(k_print_error, "vkBindBufferMemory failed: %d\n", result);
			gpu_mesh_destroy(gpu, mesh);
			return NULL;
		}
	}

	return mesh;
}

void gpu_mesh_destroy(gpu_t* gpu, gpu_mesh_t* mesh)
{
	if (mesh && mesh->index_buffer)
	{
		vkDestroyBuffer(gpu->logical_device, mesh->index_buffer, NULL);
	}
	if (mesh && mesh->index_memory)
	{
		vkFreeMemory(gpu->logical_device, mesh->index_memory, NULL);
	}
	if (mesh && mesh->vertex_buffer)
	{
		vkDestroyBuffer(gpu->logical_device, mesh->vertex_buffer, NULL);
	}
	if (mesh && mesh->vertex_memory)
	{
		vkFreeMemory(gpu->logical_device, mesh->vertex_memory, NULL);
	}
	if (mesh)
	{
		heap_free(gpu->heap, mesh);
	}
}

gpu_pipeline_t* gpu_pipeline_create(gpu_t* gpu, const gpu_pipeline_info_t* info)
{
	gpu_pipeline_t* pipeline = (gpu_pipeline_t*) heap_alloc(gpu->heap, sizeof(gpu_pipeline_t), 8);
	memset(pipeline, 0, sizeof(*pipeline));

	VkPipelineRasterizationStateCreateInfo rasterization_state_info;
	rasterization_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterization_state_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state_info.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState color_blend_state;
	color_blend_state.colorWriteMask = 0xf;
	color_blend_state.blendEnable = VK_FALSE;
	
	VkPipelineColorBlendStateCreateInfo color_blend_info;
	color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_info.attachmentCount = 1;
	color_blend_info.pAttachments = &color_blend_state;

	VkPipelineViewportStateCreateInfo viewport_state_info;
	viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_info.viewportCount = 1;
	viewport_state_info.scissorCount = 1;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info;
	depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_info.depthTestEnable = VK_TRUE;
	depth_stencil_info.depthWriteEnable = VK_TRUE;
	depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_info.back.failOp = VK_STENCIL_OP_KEEP;
	depth_stencil_info.back.passOp = VK_STENCIL_OP_KEEP;
	depth_stencil_info.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depth_stencil_info.front.failOp = VK_STENCIL_OP_KEEP;
	depth_stencil_info.front.passOp = VK_STENCIL_OP_KEEP;
	depth_stencil_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
	depth_stencil_info.stencilTestEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisample_info;
	multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineShaderStageCreateInfo shader_info[2];
	
	shader_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_info[0].module = info->shader->vertex_module;
	shader_info[0].pName = "main";
	
	shader_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_info[1].module = info->shader->fragment_module;
	shader_info[1].pName = "main";

	VkDynamicState dynamic_states[2];
	dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;

	VkPipelineDynamicStateCreateInfo dynamic_info;
	dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_info.dynamicStateCount = _countof(dynamic_states);
	dynamic_info.pDynamicStates = dynamic_states;

	VkPipelineLayoutCreateInfo pipeline_layout_info;
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &info->shader->descriptor_set_layout;
	
	VkResult result = vkCreatePipelineLayout(gpu->logical_device, &pipeline_layout_info, NULL, &pipeline->pipeline_layout);
	if (result)
	{
		debug_print(k_print_error, "vkCreatePipelineLayout failed: %d\n", result);
		gpu_pipeline_destroy(gpu, pipeline);
		return NULL;
	}

	VkGraphicsPipelineCreateInfo pipeline_info;
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.layout = pipeline->pipeline_layout;
	pipeline_info.renderPass = gpu->render_pass;
	pipeline_info.stageCount = _countof(shader_info);
	pipeline_info.pStages = shader_info;
	pipeline_info.pVertexInputState = &gpu->mesh_vertex_input_info[info->mesh_layout];
	pipeline_info.pInputAssemblyState = &gpu->mesh_input_assembly_info[info->mesh_layout];
	pipeline_info.pRasterizationState = &rasterization_state_info;
	pipeline_info.pColorBlendState = &color_blend_info;
	pipeline_info.pMultisampleState = &multisample_info;
	pipeline_info.pViewportState = &viewport_state_info;
	pipeline_info.pDepthStencilState = &depth_stencil_info;
	pipeline_info.pDynamicState = &dynamic_info;
	
	result = vkCreateGraphicsPipelines(gpu->logical_device, NULL, 1, &pipeline_info, NULL, &pipeline->pipe);
	if (result)
	{
		debug_print(k_print_error, "vkCreateGraphicsPipelines failed: %d\n", result);
		gpu_pipeline_destroy(gpu, pipeline);
		return NULL;
	}

	return pipeline;
}

void gpu_pipeline_destroy(gpu_t* gpu, gpu_pipeline_t* pipeline)
{
	if (pipeline && pipeline->pipeline_layout)
	{
		vkDestroyPipelineLayout(gpu->logical_device, pipeline->pipeline_layout, NULL);
	}
	if (pipeline && pipeline->pipe)
	{
		vkDestroyPipeline(gpu->logical_device, pipeline->pipe, NULL);
	}
	if (pipeline)
	{
		heap_free(gpu->heap, pipeline);
	}
}

gpu_shader_t* gpu_shader_create(gpu_t* gpu, const gpu_shader_info_t* info)
{
	gpu_shader_t* shader = (gpu_shader_t*) heap_alloc(gpu->heap, sizeof(gpu_shader_t), 8);
	memset(shader, 0, sizeof(*shader));

	VkShaderModuleCreateInfo vertex_module_info;
	vertex_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertex_module_info.codeSize = info->vertex_shader_size;
	vertex_module_info.pCode = (uint32_t*) info->vertex_shader_data;

	VkResult result = vkCreateShaderModule(gpu->logical_device, &vertex_module_info, NULL, &shader->vertex_module);
	if (result)
	{
		debug_print(k_print_error, "vkCreateShaderModule failed: %d\n", result);
		gpu_shader_destroy(gpu, shader);
		return NULL;
	}

	VkShaderModuleCreateInfo fragment_module_info;
	fragment_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragment_module_info.codeSize = info->fragment_shader_size;
	fragment_module_info.pCode = (uint32_t*) info->fragment_shader_data;

	result = vkCreateShaderModule(gpu->logical_device, &fragment_module_info, NULL, &shader->fragment_module);
	if (result)
	{
		debug_print(k_print_error, "vkCreateShaderModule failed: %d\n", result);
		gpu_shader_destroy(gpu, shader);
		return NULL;
	}

	VkDescriptorSetLayoutBinding* descriptor_set_layout_bindings = (VkDescriptorSetLayoutBinding * ) alloca(sizeof(VkDescriptorSetLayoutBinding) * info->uniform_buffer_count);
	for (int i = 0; i < info->uniform_buffer_count; ++i)
	{
		//descriptor_set_layout_bindings[i] = (VkDescriptorSetLayoutBinding);
		descriptor_set_layout_bindings[i].binding = i;
		descriptor_set_layout_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_set_layout_bindings[i].descriptorCount = 1;
		descriptor_set_layout_bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info;
	descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_layout_info.bindingCount = info->uniform_buffer_count;
	descriptor_set_layout_info.pBindings = descriptor_set_layout_bindings;

	result = vkCreateDescriptorSetLayout(gpu->logical_device, &descriptor_set_layout_info, NULL, &shader->descriptor_set_layout);
	if (result)
	{
		debug_print(k_print_error, "vkCreateDescriptorSetLayout failed: %d\n", result);
		gpu_shader_destroy(gpu, shader);
		return NULL;
	}

	return shader;
}

void gpu_shader_destroy(gpu_t* gpu, gpu_shader_t* shader)
{
	if (shader && shader->vertex_module)
	{
		vkDestroyShaderModule(gpu->logical_device, shader->vertex_module, NULL);
	}
	if (shader && shader->fragment_module)
	{
		vkDestroyShaderModule(gpu->logical_device, shader->fragment_module, NULL);
	}
	if (shader && shader->descriptor_set_layout)
	{
		vkDestroyDescriptorSetLayout(gpu->logical_device, shader->descriptor_set_layout, NULL);
	}
	if (shader)
	{
		heap_free(gpu->heap, shader);
	}
}

gpu_uniform_buffer_t* gpu_uniform_buffer_create(gpu_t* gpu, const gpu_uniform_buffer_info_t* info)
{
	gpu_uniform_buffer_t* uniform_buffer = (gpu_uniform_buffer_t * ) heap_alloc(gpu->heap, sizeof(gpu_uniform_buffer_t), 8);
	memset(uniform_buffer, 0, sizeof(*uniform_buffer));

	VkBufferCreateInfo buffer_info;
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = info->size;
	buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	
	VkResult result = vkCreateBuffer(gpu->logical_device, &buffer_info, NULL, &uniform_buffer->buffer);
	if (result)
	{
		debug_print(k_print_error, "vkCreateBuffer failed: %d\n", result);
		gpu_uniform_buffer_destroy(gpu, uniform_buffer);
		return NULL;
	}

	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(gpu->logical_device, uniform_buffer->buffer, &mem_reqs);

	VkMemoryAllocateInfo mem_alloc;
	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.allocationSize = mem_reqs.size;
	mem_alloc.memoryTypeIndex = get_memory_type_index(gpu, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	
	result = vkAllocateMemory(gpu->logical_device, &mem_alloc, NULL, &uniform_buffer->memory);
	if (result)
	{
		debug_print(k_print_error, "vkAllocateMemory failed: %d\n", result);
		gpu_uniform_buffer_destroy(gpu, uniform_buffer);
		return NULL;
	}

	result = vkBindBufferMemory(gpu->logical_device, uniform_buffer->buffer, uniform_buffer->memory, 0);
	if (result)
	{
		debug_print(k_print_error, "vkBindBufferMemory failed: %d\n", result);
		gpu_uniform_buffer_destroy(gpu, uniform_buffer);
		return NULL;
	}

	uniform_buffer->descriptor.buffer = uniform_buffer->buffer;
	uniform_buffer->descriptor.range = info->size;

	gpu_uniform_buffer_update(gpu, uniform_buffer, info->data, info->size);

	return uniform_buffer;
}

void gpu_uniform_buffer_update(gpu_t* gpu, gpu_uniform_buffer_t* buffer, const void* data, size_t size)
{
	void* dest = NULL;
	VkResult result = vkMapMemory(gpu->logical_device, buffer->memory, 0, size, 0, &dest);
	if (!result)
	{
		memcpy(dest, data, size);
		vkUnmapMemory(gpu->logical_device, buffer->memory);
	}
}

void gpu_uniform_buffer_destroy(gpu_t* gpu, gpu_uniform_buffer_t* buffer)
{
	if (buffer && buffer->buffer)
	{
		vkDestroyBuffer(gpu->logical_device, buffer->buffer, NULL);
	}
	if (buffer && buffer->memory)
	{
		vkFreeMemory(gpu->logical_device, buffer->memory, NULL);
	}
	if (buffer)
	{
		heap_free(gpu->heap, buffer);
	}
}

gpu_cmd_buffer_t* gpu_frame_begin(gpu_t* gpu)
{
	gpu_frame_t* frame = &gpu->frames[gpu->frame_index];

	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkResult result = vkBeginCommandBuffer(frame->cmd_buffer->buffer, &begin_info);
	if (result)
	{
		debug_print(k_print_error, "vkBeginCommandBuffer failed: %d\n", result);
		return NULL;
	}

	VkClearValue clear_values[2];
	//{
	//	{.color = {.float32 = { 0.0f, 0.0f, 0.2f, 1.0f } } },
	//	{.depthStencil = {.depth = 1.0f, .stencil = 0 } },
	//};

	clear_values[0].color.float32[0] = 0.0f;
	clear_values[0].color.float32[1] = 0.0f;
	clear_values[0].color.float32[2] = 0.2f;
	clear_values[0].color.float32[3] = 1.0f;

	clear_values[1].depthStencil.depth = 1.0f;
	clear_values[1].depthStencil.stencil = 0;

	VkRenderPassBeginInfo render_pass_begin_info;
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.renderPass = gpu->render_pass;
	render_pass_begin_info.renderArea.extent.width = gpu->frame_width;
	render_pass_begin_info.renderArea.extent.height = gpu->frame_height;
	render_pass_begin_info.clearValueCount = _countof(clear_values);
	render_pass_begin_info.pClearValues = clear_values;
	render_pass_begin_info.framebuffer = frame->frame_buffer;
	
	vkCmdBeginRenderPass(frame->cmd_buffer->buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport;
	viewport.height = (float)gpu->frame_height;
	viewport.width = (float)gpu->frame_width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	
	vkCmdSetViewport(frame->cmd_buffer->buffer, 0, 1, &viewport);

	VkRect2D scissor;
	scissor.extent.width = gpu->frame_width;
	scissor.extent.height = gpu->frame_height;
	
	vkCmdSetScissor(frame->cmd_buffer->buffer, 0, 1, &scissor);

	return frame->cmd_buffer;
}

void gpu_frame_end(gpu_t* gpu)
{
	gpu_frame_t* frame = &gpu->frames[gpu->frame_index];
	gpu->frame_index = (gpu->frame_index + 1) % gpu->frame_count;

	vkCmdEndRenderPass(frame->cmd_buffer->buffer);
	VkResult result = vkEndCommandBuffer(frame->cmd_buffer->buffer);
	if (result)
	{
		debug_print(k_print_error, "vkEndCommandBuffer failed: %d\n", result);
	}

	uint32_t image_index;
	result = vkAcquireNextImageKHR(gpu->logical_device, gpu->swap_chain, UINT64_MAX, gpu->present_complete_sema, VK_NULL_HANDLE, &image_index);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		debug_print(k_print_error, "vkAcquireNextImageKHR failed: %d\n", result);
	}

	result = vkWaitForFences(gpu->logical_device, 1, &frame->fence, VK_TRUE, UINT64_MAX);
	if (result)
	{
		debug_print(k_print_error, "vkWaitForFences failed: %d\n", result);
	}
	result = vkResetFences(gpu->logical_device, 1, &frame->fence);
	if (result)
	{
		debug_print(k_print_error, "vkResetFences failed: %d\n", result);
	}

	VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pWaitDstStageMask = &wait_stage_mask;
	submit_info.waitSemaphoreCount = 1;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pCommandBuffers = &frame->cmd_buffer->buffer;
	submit_info.commandBufferCount = 1;
	submit_info.pWaitSemaphores = &gpu->present_complete_sema;
	submit_info.pSignalSemaphores = &gpu->render_complete_sema;
	
	result = vkQueueSubmit(gpu->queue, 1, &submit_info, frame->fence);
	if (result)
	{
		debug_print(k_print_error, "vkQueueSubmit failed: %d\n", result);
	}

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &gpu->swap_chain;
	present_info.pImageIndices = &image_index;
	present_info.pWaitSemaphores = &gpu->render_complete_sema;
	present_info.waitSemaphoreCount = 1;
	
	result = vkQueuePresentKHR(gpu->queue, &present_info);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		debug_print(k_print_error, "vkQueuePresentKHR failed: %d\n", result);
	}
}

void gpu_cmd_pipeline_bind(gpu_t* gpu, gpu_cmd_buffer_t* cmd_buffer, gpu_pipeline_t* pipeline)
{
	vkCmdBindPipeline(cmd_buffer->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipe);
	cmd_buffer->pipeline_layout = pipeline->pipeline_layout;
}

void gpu_cmd_descriptor_bind(gpu_t* gpu, gpu_cmd_buffer_t* cmd_buffer, gpu_descriptor_t* descriptor)
{
	vkCmdBindDescriptorSets(cmd_buffer->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd_buffer->pipeline_layout, 0, 1, &descriptor->set, 0, NULL);
}

void gpu_cmd_mesh_bind(gpu_t* gpu, gpu_cmd_buffer_t* cmd_buffer, gpu_mesh_t* mesh)
{
	if (mesh->vertex_count)
	{
		VkDeviceSize zero = 0;
		vkCmdBindVertexBuffers(cmd_buffer->buffer, 0, 1, &mesh->vertex_buffer, &zero);
		cmd_buffer->vertex_count = mesh->vertex_count;
	}
	else
	{
		cmd_buffer->vertex_count = 0;
	}
	if (mesh->index_count)
	{
		vkCmdBindIndexBuffer(cmd_buffer->buffer, mesh->index_buffer, 0, mesh->index_type);
		cmd_buffer->index_count = mesh->index_count;
	}
	else
	{
		cmd_buffer->index_count = 0;
	}
}

void gpu_cmd_draw(gpu_t* gpu, gpu_cmd_buffer_t* cmd_buffer)
{
	if (cmd_buffer->index_count)
	{
		vkCmdDrawIndexed(cmd_buffer->buffer, cmd_buffer->index_count, 1, 0, 0, 0);
	}
	else if (cmd_buffer->vertex_count)
	{
		vkCmdDraw(cmd_buffer->buffer, cmd_buffer->vertex_count, 1, 0, 0);
	}
}

void gpu_pass_info_to_gui(gpu_t* gpu, gui_init_info_t* user)
{
	user->instance = gpu->instance;
	user->physical_device = gpu->physical_device;
	user->device = gpu->logical_device;
	user->queue_family = gpu->queue_family;
	user->queue = gpu->queue;
	user->pipeline_cache = VK_NULL_HANDLE;
	user->descriptor_pool = gpu->descriptor_pool;
	user->subpass = 0;
	//user->msaa_samples = VK_SAMPLE_COUNT_1_BIT;
	user->allocator = VK_NULL_HANDLE;
	user->check_result = VK_NULL_HANDLE;
	user->swap_chain = gpu->swap_chain;
	user->command_pool = gpu->cmd_pool;
	user->width = gpu->frame_width;
	user->height = gpu->frame_height;
	user->surface = gpu->surface;
}

static void create_mesh_layouts(gpu_t* gpu)
{
	// k_gpu_mesh_layout_tri_p444_i2
	{
		//gpu->mesh_input_assembly_info[k_gpu_mesh_layout_tri_p444_i2] = (VkPipelineInputAssemblyStateCreateInfo)
		gpu->mesh_input_assembly_info[k_gpu_mesh_layout_tri_p444_i2].sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		gpu->mesh_input_assembly_info[k_gpu_mesh_layout_tri_p444_i2].topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		

		VkVertexInputBindingDescription* vertex_binding = (VkVertexInputBindingDescription * ) heap_alloc(gpu->heap, sizeof(VkVertexInputBindingDescription), 8);
		//*vertex_binding = (VkVertexInputBindingDescription)
		vertex_binding->binding = 0;
		vertex_binding->stride = 12;
		vertex_binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		

		VkVertexInputAttributeDescription* vertex_attributes = (VkVertexInputAttributeDescription * ) heap_alloc(gpu->heap, sizeof(VkVertexInputAttributeDescription) * 1, 8);
		//vertex_attributes[0] = (VkVertexInputAttributeDescription)
		vertex_attributes[0].binding = 0;
		vertex_attributes[0].location = 0;
		vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attributes[0].offset = 0;

		//gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_i2] = (VkPipelineVertexInputStateCreateInfo)
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_i2].sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_i2].vertexBindingDescriptionCount = 1,
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_i2].pVertexBindingDescriptions = vertex_binding,
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_i2].vertexAttributeDescriptionCount = 1,
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_i2].pVertexAttributeDescriptions = vertex_attributes,

		gpu->mesh_index_type[k_gpu_mesh_layout_tri_p444_i2] = VK_INDEX_TYPE_UINT16;
		gpu->mesh_index_size[k_gpu_mesh_layout_tri_p444_i2] = (VkIndexType) 2;
		gpu->mesh_vertex_size[k_gpu_mesh_layout_tri_p444_i2] = (VkIndexType) 12;
	}

	// k_gpu_mesh_layout_tri_p444_c444_i2
	{
		//gpu->mesh_input_assembly_info[k_gpu_mesh_layout_tri_p444_c444_i2] = (VkPipelineInputAssemblyStateCreateInfo)
		gpu->mesh_input_assembly_info[k_gpu_mesh_layout_tri_p444_c444_i2].sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		gpu->mesh_input_assembly_info[k_gpu_mesh_layout_tri_p444_c444_i2].topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;


		VkVertexInputBindingDescription* vertex_binding = (VkVertexInputBindingDescription * ) heap_alloc(gpu->heap, sizeof(VkVertexInputBindingDescription), 8);
		//*vertex_binding = (VkVertexInputBindingDescription)
		vertex_binding->binding = 0;
		vertex_binding->stride = 24;
		vertex_binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription* vertex_attributes = (VkVertexInputAttributeDescription*) heap_alloc(gpu->heap, sizeof(VkVertexInputAttributeDescription) * 2, 8);
		//vertex_attributes[0] = (VkVertexInputAttributeDescription)
		vertex_attributes[0].binding = 0;
		vertex_attributes[0].location = 0;
		vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attributes[0].offset = 0;
		
		//vertex_attributes[1] = (VkVertexInputAttributeDescription)
		vertex_attributes[1].binding = 0;
		vertex_attributes[1].location = 1;
		vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attributes[1].offset = 12;

		//gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_c444_i2] = (VkPipelineVertexInputStateCreateInfo)
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_c444_i2].sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_c444_i2].vertexBindingDescriptionCount = 1;
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_c444_i2].pVertexBindingDescriptions = vertex_binding;
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_c444_i2].vertexAttributeDescriptionCount = 2;
		gpu->mesh_vertex_input_info[k_gpu_mesh_layout_tri_p444_c444_i2].pVertexAttributeDescriptions = vertex_attributes;

		gpu->mesh_index_type[k_gpu_mesh_layout_tri_p444_c444_i2] = VK_INDEX_TYPE_UINT16;
		gpu->mesh_index_size[k_gpu_mesh_layout_tri_p444_c444_i2] = (VkIndexType) 2;
		gpu->mesh_vertex_size[k_gpu_mesh_layout_tri_p444_c444_i2] = (VkIndexType)24;
	}
}

static void destroy_mesh_layouts(gpu_t* gpu)
{
	for (int i = 0; i < _countof(gpu->mesh_vertex_input_info); ++i)
	{
		if (gpu->mesh_vertex_input_info[i].pVertexBindingDescriptions)
		{
			heap_free(gpu->heap, (void*)gpu->mesh_vertex_input_info[i].pVertexBindingDescriptions);
			heap_free(gpu->heap, (void*)gpu->mesh_vertex_input_info[i].pVertexAttributeDescriptions);
		}
	}
}

static uint32_t get_memory_type_index(gpu_t* gpu, uint32_t bits, VkMemoryPropertyFlags properties)
{
	for (uint32_t i = 0; i < gpu->memory_properties.memoryTypeCount; ++i)
	{
		if (bits & (1UL << i))
		{
			if ((gpu->memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
	}
	debug_print(k_print_error, "Unable to find memory of type: %x\n", bits);
	return 0;
}
