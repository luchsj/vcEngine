#include "ecs.h"
#include "fs.h"
#include "debug.h"
#include "gpu.h"
#include "heap.h"
#include "frogger_game.h"
#include "render.h"
#include "timer_object.h"
#include "transform.h"
#include "wm.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

typedef struct transform_component_t
{
	transform_t transform;
} transform_component_t;

typedef struct camera_component_t
{
	mat4f_t projection;
	mat4f_t view;
} camera_component_t;

typedef struct model_component_t
{
	gpu_mesh_info_t* mesh_info;
	gpu_shader_info_t* shader_info;
} model_component_t;

typedef struct material_component_t
{
	vec3f_t rgb;
}material_component_t;

typedef struct player_component_t
{
	int index;
	int speed;
	float hitbox_h;
	float hitbox_w;
	transform_t respawn_pos;
} player_component_t;

typedef struct car_component_t
{
	int index;
	float speed;
	float hitbox_h;
	float hitbox_w;
	float bound_w; //width of lane across the screen
} car_component_t;

typedef struct name_component_t
{
	char name[32];
} name_component_t;

typedef struct frogger_game_t
{
	heap_t* heap;
	fs_t* fs;
	wm_window_t* window;
	render_t* render;

	timer_object_t* timer;

	ecs_t* ecs;
	int transform_type;
	int camera_type;
	int model_type;
	int material_type;
	int player_type;
	int car_type;
	int name_type;
	ecs_entity_ref_t player_ent;
	ecs_entity_ref_t camera_ent;

	ecs_entity_ref_t car_ent;

	gpu_mesh_info_t cube_mesh;
	gpu_shader_info_t cube_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;
}frogger_game_t;

static void load_resources(frogger_game_t* game);
static void unload_resources(frogger_game_t* game);
static void spawn_car(frogger_game_t* game, int index, int start_x, int start_y, int speed);
static void spawn_player(frogger_game_t* game, int index, int speed);
static void spawn_camera(frogger_game_t* game);
static void update_players(frogger_game_t* game);
static void update_cars(frogger_game_t* game);
static void draw_models(frogger_game_t* game);

frogger_game_t* frogger_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render)
{
	frogger_game_t* game = heap_alloc(heap, sizeof(frogger_game_t), 8);
	game->heap = heap;
	game->fs = fs;
	game->window = window;
	game->render = render;

	game->timer = timer_object_create(heap, NULL);

	game->ecs = ecs_create(heap);
	game->transform_type = ecs_register_component_type(game->ecs, "transform", sizeof(transform_component_t), _Alignof(transform_component_t));
	game->camera_type = ecs_register_component_type(game->ecs, "camera", sizeof(camera_component_t), _Alignof(camera_component_t));
	game->model_type = ecs_register_component_type(game->ecs, "model", sizeof(model_component_t), _Alignof(model_component_t));
	game->material_type = ecs_register_component_type(game->ecs, "material" , sizeof(material_component_t), _Alignof(material_component_t));
	game->player_type = ecs_register_component_type(game->ecs, "player", sizeof(player_component_t), _Alignof(player_component_t));
	game->car_type = ecs_register_component_type(game->ecs, "car", sizeof(car_component_t), _Alignof(car_component_t));
	game->name_type = ecs_register_component_type(game->ecs, "name", sizeof(name_component_t), _Alignof(name_component_t));

	load_resources(game);
	spawn_player(game, 0, 2, .25);
	//spawn_player(game, 1);

	//first row
	spawn_car(game, 0, 0, 2, 2);
	spawn_car(game, 1, -5, 2, 2);
	spawn_car(game, 2, 5, 2, 2);

	//second row
	spawn_car(game, 3, 0, 0, -5);
	spawn_car(game, 4, -2, 0, -5);
	spawn_car(game, 5, 4, 0, -5);
	spawn_car(game, 6, 8, 0, -5);
	spawn_car(game, 7, 6, 0, -5);

	//third row
	spawn_car(game, 8, -2, -2, 8);
	spawn_car(game, 9, 8, -2, 8);
	spawn_car(game, 10, 6, -2, 8);

	spawn_camera(game);

	return game;
}

void frogger_game_destroy(frogger_game_t* game)
{
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

void frogger_game_update(frogger_game_t* game)
{
	timer_object_update(game->timer);
	ecs_update(game->ecs);
	update_players(game);
	update_cars(game);
	draw_models(game);
	//render_push_ui(game->render);
	render_push_done(game->render);
}

static void load_resources(frogger_game_t* game)
{
	game->vertex_shader_work = fs_read(game->fs, "shaders/triangle.vert.spv", game->heap, false, false);
	game->fragment_shader_work = fs_read(game->fs, "shaders/triangle.frag.spv", game->heap, false, false);
	game->cube_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	static vec3f_t cube_verts[] =
	{
		{ -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f,  1.0f }, //{pos}, {color}
		{  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f,  1.0f },
		{  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f,  0.0f },
		{ -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f,  1.0f },
		{  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f,  1.0f },
		{ -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f,  0.0f },
	};
	static uint16_t cube_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	game->cube_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts,
		.vertex_data_size = sizeof(cube_verts),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};
}

static void unload_resources(frogger_game_t* game)
{
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

static void spawn_player(frogger_game_t* game, int index, int speed, float scale)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->material_type) |
		(1ULL << game->name_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.z = 4.0f; //should add var for start pos
	transform_comp->transform.scale.x = scale;
	transform_comp->transform.scale.y = scale;
	transform_comp->transform.scale.z = scale;

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "player");

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;
	player_comp->speed = speed;
	player_comp->hitbox_h = transform_comp->transform.scale.z;
	player_comp->hitbox_w = transform_comp->transform.scale.y;
	player_comp->respawn_pos = transform_comp->transform;
	transform_comp->transform.scale.x = 0.25f;
	transform_comp->transform.scale.y = 0.25f;
	transform_comp->transform.scale.z = 0.25f;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;

	material_component_t* material_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->material_type, true);
	material_comp->rgb.x = 0; material_comp->rgb.y = 1; material_comp->rgb.z = 0; 
}

static void spawn_car(frogger_game_t* game, int index, int start_x, int start_y, int speed)
{
	uint64_t k_car_ent_mask = 
		(1ULL << game->transform_type) | 
		(1ULL << game->name_type) | 
		(1ULL << game->model_type) | 
		(1ULL << game->material_type) | 
		(1ULL << game->car_type);
	game->car_ent = ecs_entity_add(game->ecs, k_car_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->car_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	//this index thing with y being x and z being y is weird and is something i'd like to address later
	transform_comp->transform.translation.y = start_x; 
	transform_comp->transform.translation.z = start_y;
	transform_comp->transform.scale.y = .5f + .25f * (index % 2);
	transform_comp->transform.scale.z = .5f;

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->car_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "car");

	car_component_t* car_comp = ecs_entity_get_component(game->ecs, game->car_ent, game->car_type, true);
	car_comp->index = index;
	car_comp->speed = speed;
	car_comp->bound_w = 10.0f;
	car_comp->hitbox_h = transform_comp->transform.scale.z;// *0.5f;
	car_comp->hitbox_w = transform_comp->transform.scale.y;// *0.5f;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->car_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;

	material_component_t* material_comp = ecs_entity_get_component(game->ecs, game->car_ent, game->material_type, true);
	material_comp->rgb.x = 1; material_comp->rgb.y = 0; material_comp->rgb.z = 0; 
}

static void spawn_camera(frogger_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	//mat4f_make_perspective(&camera_comp->projection, (float)M_PI / 2.0f, 16.0f / 9.0f, 0.1f, 100.0f);
	mat4f_make_orthographic(&camera_comp->projection, -8.0f, 8.0f, 4.5f, -4.5f, 0.1f, 10.0f);

	vec3f_t eye_pos = vec3f_scale(vec3f_forward(), -10.0f);
	vec3f_t forward = vec3f_forward();
	vec3f_t up = vec3f_up();
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

static void update_players(frogger_game_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		player_component_t* player_comp = ecs_query_get_component(game->ecs, &query, game->player_type);

		transform_t move;
		transform_identity(&move);
		if (key_mask & k_key_up)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), -dt * player_comp->speed));
		}
		if (key_mask & k_key_down)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), dt * player_comp->speed));
		}
		if (key_mask & k_key_left)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -dt * player_comp->speed));
		}
		if (key_mask & k_key_right)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), dt * player_comp->speed));
		}
		transform_multiply(&transform_comp->transform, &move);

		//respawn player on win
		if (transform_comp->transform.translation.z < -3.0f)
			transform_comp->transform = player_comp->respawn_pos;
	}
}

static void update_cars(frogger_game_t* game)
{
	float dt = (float) timer_object_get_delta_ms(game->timer) * .001f;
	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->car_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask); ecs_query_is_valid(game->ecs, &query); ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		car_component_t* car_comp = ecs_query_get_component(game->ecs, &query, game->car_type);

		transform_t move;
		transform_identity(&move);
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -dt * car_comp->speed));

		if (transform_comp->transform.translation.y < -car_comp->bound_w)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), car_comp->bound_w * 2));
		}
		else if (transform_comp->transform.translation.y > car_comp->bound_w)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -car_comp->bound_w * 2));
		}

		transform_multiply(&transform_comp->transform, &move);
		
		//collision check
		transform_component_t* player_transform = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
		player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);

		if (fabs(player_transform->transform.translation.z - transform_comp->transform.translation.z) < car_comp->hitbox_h + player_comp->hitbox_h
			&& fabs(player_transform->transform.translation.y - transform_comp->transform.translation.y) < car_comp->hitbox_w + player_comp->hitbox_w)
		{
			player_transform->transform = player_comp->respawn_pos;
		}
	}
}

static void draw_models(frogger_game_t* game)
{
	uint64_t k_camera_query_mask = (1ULL << game->camera_type);
	for (ecs_query_t camera_query = ecs_query_create(game->ecs, k_camera_query_mask);
		ecs_query_is_valid(game->ecs, &camera_query);
		ecs_query_next(game->ecs, &camera_query))
	{
		camera_component_t* camera_comp = ecs_query_get_component(game->ecs, &camera_query, game->camera_type);

		uint64_t k_model_query_mask = (1ULL << game->transform_type) | (1ULL << game->model_type) | (1ULL << game->material_type);
		for (ecs_query_t query = ecs_query_create(game->ecs, k_model_query_mask);
			ecs_query_is_valid(game->ecs, &query);
			ecs_query_next(game->ecs, &query))
		{
			transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
			model_component_t* model_comp = ecs_query_get_component(game->ecs, &query, game->model_type);
			material_component_t* material_comp = ecs_query_get_component(game->ecs, &query, game->material_type);
			ecs_entity_ref_t entity_ref = ecs_query_get_entity(game->ecs, &query);

			struct
			{
				mat4f_t projection;
				mat4f_t model;
				mat4f_t view;
				vec3f_t rgb; 
			} uniform_data;
			uniform_data.projection = camera_comp->projection;
			uniform_data.view = camera_comp->view;
			uniform_data.rgb = material_comp->rgb;
			transform_to_matrix(&transform_comp->transform, &uniform_data.model);
			gpu_uniform_buffer_info_t uniform_info = { .data = &uniform_data, sizeof(uniform_data) };

			render_push_model(game->render, &entity_ref, model_comp->mesh_info, model_comp->shader_info, &uniform_info);
		}
	}
}
