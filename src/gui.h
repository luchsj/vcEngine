#pragma once
//UI system
//Mainly wrappers for Dear ImGui calls.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gui_t gui_t;
typedef struct heap_t heap_t;
typedef struct wm_window_t wm_window_t;
typedef struct gpu_t gpu_t;

/*
typedef enum gui_system_element_t
{
	k_element_text = 1 << 0
}gui_system_element_t;
*/
//initialize GUI
gui_t* gui_init(heap_t* heap, wm_window_t* window, gpu_t* gpu);

//draw each element in the UI. called each frame in the render loop
void gui_push_ui_to_render(gui_t* gui);

//add a new element to be drawn in ui_draw
//void gui_add_element(gui_system_element_t element);
#ifdef __cplusplus
}
#endif