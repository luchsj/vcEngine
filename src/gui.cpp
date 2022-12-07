#include "debug.h"
#include "gui.h"
#include "gui_helper.h"
#include "gpu.h"
#include "heap.h"
#include "render.h"
#include "wm.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#include "imgui/backends/imgui_impl_win32.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "GUI Vulkan failure: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

typedef struct gui_t
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
	VkAllocationCallbacks* allocator;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	uint32_t queue_family;
	VkQueue queue;
	VkDebugReportCallbackEXT debug_report;
	VkPipelineCache pipeline_cache;
	VkDescriptorPool descriptor_pool;
	heap_t* heap;
	VkSwapchainKHR swap_chain;

	VkRenderPass render_pass;
	ImGui_ImplVulkanH_Window* window_data;
	int min_image_count;
	bool swap_chain_rebuild = false;

	uint32_t frame_height;
	uint32_t frame_width;
#endif
}gui_t;

gui_t* gui_init(heap_t * heap, wm_window_t * window, gpu_t * gpu)
{
	VkResult err; 

	gui_t* new_sys = (gui_t*) heap_alloc(heap, sizeof(gui_t), 8);
	if (!new_sys)
	{
		debug_print(k_print_error, "gui_init failed: memory allocation returned NULL\n");
		return NULL;
	}

	new_sys->heap = heap;

	// Create a new Vulkan descriptor pool for ImGui
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = 11;
	pool_info.pPoolSizes = pool_sizes;

	// Set default values for new_sys
	{
	new_sys->allocator = NULL;
	new_sys->instance = VK_NULL_HANDLE;
	new_sys->physical_device = VK_NULL_HANDLE;
	new_sys->device = VK_NULL_HANDLE;
	new_sys->queue_family = (uint32_t)-1;
	new_sys->queue = VK_NULL_HANDLE;
	new_sys->debug_report = VK_NULL_HANDLE;
	new_sys->pipeline_cache = VK_NULL_HANDLE;
	new_sys->descriptor_pool = VK_NULL_HANDLE;
	new_sys->swap_chain = VK_NULL_HANDLE;
	}

	// Get information from the GPU
	{
		gui_init_info_t info_helper;
		gpu_pass_info_to_gui(gpu, &info_helper);

		new_sys->instance = (VkInstance)info_helper.instance;
		new_sys->physical_device = (VkPhysicalDevice)info_helper.physical_device;
		new_sys->device = (VkDevice)info_helper.device;
		new_sys->queue_family = info_helper.queue_family;
		new_sys->queue = (VkQueue)info_helper.queue;
		new_sys->pipeline_cache = VK_NULL_HANDLE;
		new_sys->descriptor_pool = (VkDescriptorPool)info_helper.descriptor_pool;
		new_sys->swap_chain = (VkSwapchainKHR)info_helper.swap_chain;
		new_sys->frame_height = info_helper.height;
		new_sys->frame_width = info_helper.width;

		new_sys->min_image_count = 2;
		new_sys->swap_chain_rebuild = false;
	

	// Initialize window data 
	new_sys->window_data = (ImGui_ImplVulkanH_Window*) heap_alloc(new_sys->heap, sizeof(ImGui_ImplVulkanH_Window), 8);
	new_sys->window_data->Surface = (VkSurfaceKHR) info_helper.surface;
	}

	// Initialize descriptor pool
	vkCreateDescriptorPool(new_sys->device, &pool_info, nullptr, &new_sys->descriptor_pool);

	//ImGui_ImplWin32_Init(wm_get_raw_window(window));
	//new_sys->window_data->Surface;
	//todo: gpu.c needs additional function to return reference to surface
	//exposure concerns?
	//imgui should stay on same thread as entities
	//imgui's output data should then be given to the render thread - gonna be a challenge to sync!

	// Initialize render pass
	{
	VkAttachmentDescription attachment = {};
	attachment.format = VK_FORMAT_B8G8R8A8_SRGB; //where can we unhardcode this???
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment = {};
	color_attachment.attachment = 0;
	color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;
	if (vkCreateRenderPass(new_sys->device, &info, nullptr, &new_sys->render_pass) != VK_SUCCESS) {
		debug_print(k_print_error, "GUI init failure: failed to initialize render pass\n");
	}
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();

	// Initialize Win32
	ImGui_ImplWin32_Init(wm_get_raw_window(window));

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = new_sys->instance;
	init_info.Allocator = VK_NULL_HANDLE;
	init_info.PhysicalDevice = new_sys->physical_device;
	init_info.Device = new_sys->device;
	init_info.QueueFamily = new_sys->queue_family;
	init_info.Queue = (VkQueue)new_sys->queue;
	init_info.DescriptorPool = new_sys->descriptor_pool;
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	//g_MainWindowData.Surface = (VkSurfaceKHR) gpu_window_info(gpu);
	/*
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
	*/

	ImGui_ImplVulkan_Init(&init_info, new_sys->render_pass);

	/*
	// Upload ImGui fonts to GPU
	VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
	VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

	vkResetCommandPool(new_sys->device, command_pool, 0);
	//check_vk_result(err);
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);
	//check_vk_result(err);

	ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

	VkSubmitInfo end_info = {};
	end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	end_info.commandBufferCount = 1;
	end_info.pCommandBuffers = &command_buffer;
	vkEndCommandBuffer(command_buffer);
	//check_vk_result(err);
	vkQueueSubmit(new_sys->queue, 1, &end_info, VK_NULL_HANDLE);
	//check_vk_result(err);

	vkDeviceWaitIdle(new_sys->device);
	//check_vk_result(err);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
	
	*/
	return new_sys;
}

void gui_font_init(gui_t* gui)
{
	// Upload Fonts
	VkResult err;
	// Use any command queue
	VkCommandPool command_pool = gui->window_data->Frames[gui->window_data->FrameIndex].CommandPool;
	VkCommandBuffer command_buffer = gui->window_data->Frames[gui->window_data->FrameIndex].CommandBuffer;

	err = vkResetCommandPool(gui->device, command_pool, 0);
	check_vk_result(err);
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	err = vkBeginCommandBuffer(command_buffer, &begin_info);
	check_vk_result(err);

	ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

	VkSubmitInfo end_info = {};
	end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	end_info.commandBufferCount = 1;
	end_info.pCommandBuffers = &command_buffer;
	err = vkEndCommandBuffer(command_buffer);
	check_vk_result(err);
	err = vkQueueSubmit(gui->queue, 1, &end_info, VK_NULL_HANDLE);
	check_vk_result(err);

	err = vkDeviceWaitIdle(gui->device);
	check_vk_result(err);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void gui_render(gui_t* gui)
{
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	//Resize swap chain if necessary
	if (gui->swap_chain_rebuild)
	{
		gui->swap_chain_rebuild = false;
		ImGui_ImplVulkan_SetMinImageCount(gui->min_image_count);
		ImGui_ImplVulkanH_CreateOrResizeWindow(gui->instance, gui->physical_device, gui->device, gui->window_data, gui->queue_family, gui->allocator, gui->frame_width, gui->frame_height, gui->min_image_count);
		gui->window_data->FrameIndex = 0;
	}

	// Tell ImGui to generate a new frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();

	// Now all the API stuff :)
	VkResult err;

	VkSemaphore image_acquired_semaphore = gui->window_data->FrameSemaphores[gui->window_data->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = gui->window_data->FrameSemaphores[gui->window_data->SemaphoreIndex].RenderCompleteSemaphore;
	err = vkAcquireNextImageKHR(gui->device, gui->window_data->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &gui->window_data->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		gui->swap_chain_rebuild = true;
		return;
	}
	check_vk_result(err);

	ImGui_ImplVulkanH_Frame* fd = &gui->window_data->Frames[gui->window_data->FrameIndex];
	{
		err = vkWaitForFences(gui->device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
		check_vk_result(err);

		err = vkResetFences(gui->device, 1, &fd->Fence);
		check_vk_result(err);
	}
	{
		err = vkResetCommandPool(gui->device, fd->CommandPool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = gui->window_data->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = gui->window_data->Width;
		info.renderArea.extent.height = gui->window_data->Height;
		info.clearValueCount = 1;
		info.pClearValues = &gui->window_data->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record dear imgui primitives into command buffer
	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

	// Submit command buffer
	vkCmdEndRenderPass(fd->CommandBuffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;

		err = vkEndCommandBuffer(fd->CommandBuffer);
		check_vk_result(err);
		err = vkQueueSubmit(gui->queue, 1, &info, fd->Fence);
		check_vk_result(err);
	}
}

void gui_present(gui_t* gui)
{
	if (gui->swap_chain_rebuild)
		return;
	VkSemaphore render_complete_semaphore = gui->window_data->FrameSemaphores[gui->window_data->SemaphoreIndex].RenderCompleteSemaphore;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &render_complete_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &gui->window_data->Swapchain;
	info.pImageIndices = &gui->window_data->FrameIndex;
	VkResult err = vkQueuePresentKHR(gui->queue, &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		gui->swap_chain_rebuild = true;
		return;
	}
	check_vk_result(err);
	gui->window_data->SemaphoreIndex = (gui->window_data->SemaphoreIndex + 1) % gui->window_data->ImageCount; // Now we can use the next set of semaphores
}