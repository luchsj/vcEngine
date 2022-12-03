#include "debug.h"
#include "gui.h"
#include "gpu.h"
#include "heap.h"
#include "wm.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_vulkan.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

typedef struct gui_t
{
	ImGui_ImplVulkanH_Window* window_data;
	wm_window_t* window;
}gui_t;


gui_t * gui_init(heap_t * heap, wm_window_t * window, gpu_t * gpu)
{
	gui_t* new_sys = (gui_t*) heap_alloc(heap, sizeof(gui_t), 8);
	if (!new_sys)
	{
		debug_print(k_print_error, "gui_init failed: memory allocation returned NULL\n");
		return NULL;
	}

	new_sys->window = window;
	//new_sys->window_data = heap_alloc(heap, sizeof(ImGui_ImplVulkanH_Window))
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();

	//ImGui_ImplWin32_Init(wm_get_raw_window(window));
	//new_sys->window_data->Surface;
	//todo: gpu.c needs additional function to return reference to surface
	//exposure concerns?
	//imgui should stay on same thread as entities
	//imgui's output data should then be given to the render thread - gonna be a challenge to sync!

	ImGui_ImplVulkan_InitInfo info = {};
	//g_MainWindowData.Surface = (VkSurfaceKHR) gpu_window_info(gpu);
	ImGui_ImplVulkan_Init(&info, new_sys->window_data->RenderPass);
	return new_sys;
}

void gui_draw_ui(gui_t* gui)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();
}
