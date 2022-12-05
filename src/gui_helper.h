#pragma once

// Helper structs for information that must be shared between the GUI system and the GPU out of necessity.

typedef struct gui_init_info_t
{
	void* instance;
	void* physical_device;
	void* device;
	uint32_t queue_family;
	void* queue;
	int min_image_count;
	void* debug_report;
	void* check_result;
	void* descriptor_pool;
	void* pipeline_cache;
	void* image_count;
	void* allocator;
	void* msaa_samples;
	uint32_t subpass;
	void* swap_chain;
}gui_init_info_t;