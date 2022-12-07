#pragma once

// High-level graphics rendering interface.

typedef struct render_t render_t;

typedef struct ecs_entity_ref_t ecs_entity_ref_t;
typedef struct gpu_mesh_info_t gpu_mesh_info_t;
typedef struct gpu_shader_info_t gpu_shader_info_t;
typedef struct gpu_uniform_buffer_info_t gpu_uniform_buffer_info_t;
typedef struct gui_t gui_t;
typedef struct heap_t heap_t;
typedef struct wm_window_t wm_window_t;
typedef struct ImDrawData ImDrawData;

#ifdef __cplusplus
extern "C" {
#endif

// Create a render system.
render_t* render_create(heap_t* heap, wm_window_t* window);

// Destroy a render system.
void render_destroy(render_t* render);

// Push a model onto a queue of items to be rendered.
void render_push_model(render_t* render, ecs_entity_ref_t* entity, gpu_mesh_info_t* mesh, gpu_shader_info_t* shader, gpu_uniform_buffer_info_t* uniform);

// Push a frame of UI onto the render queue.
void render_push_ui(render_t* render);

// Push an end-of-frame marker on a queue of items to be rendered.
void render_push_done(render_t* render);

#ifdef __cplusplus
}
#endif
