// Spdx-License-Identifier: MIT
/**
 * @file    esz.c
 * @brief   eszFW game engine
 * @details A cross-platform game engine written in C
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <SDL.h>
#include <cwalk.h>

#ifdef USE_LIBTMX
    #include <tmx.h>
#else // (cute_tiled.h)
    #define CUTE_TILED_IMPLEMENTATION
    #include <cute_tiled.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#pragma clang diagnostic pop

#include "esz.h"
#include "esz_types.h"

#define CLR_STATE(number, bit) number &= ~(1UL << bit)
#define SET_STATE(number, bit) number |=   1UL << bit
#define IS_STATE_SET(number, bit) ((0U == (number & (1 << bit))) ? 0U : 1U)

// Hash table
// ----------------------------------------------------------------------------

#define H_acceleration                 0xce26e518186a848f
#define H_animated_tile_fps            0xf16ba347de2debdd
#define H_background_constant_velocity 0x1cb19bb15ad8b7fc
#define H_background_is_top_aligned    0xe10d87b900773f85
#define H_background_layer_shift       0xf42f15f4c255007e
#define H_climbable                    0x0377c455420b8600
#define H_connect_horizontal_map_ends  0xb2d77d5c88cb679e
#define H_connect_vertical_map_ends    0x8fd9bf6992bca50e
#define H_actor                        0x000000310f128ebe
#define H_gravitation                  0xc090e5ec12404d2d
#define H_height                       0x0000065301d688de
#define H_is_affected_by_gravity       0xd7df2608f228f6d1
#define H_is_animated                  0xc09beeb13eae4983
#define H_is_in_background             0xdba806855b4839b6
#define H_is_in_foreground             0xdba8c1d8d43eb8f1
#define H_is_in_midground              0x7b0525da0f6ff45f
#define H_is_left_oriented             0x1af19f00c7d179a4
#define H_is_moving                    0x0377cc4471f37f30
#define H_is_player                    0x0377cc4478b16e8d
#define H_jumping_power                0x702da8a7606d92ab
#define H_max_velocity_x               0xa1d4b1b096163590
#define H_meter_in_pixel               0xfbbc8a6d4a407cf9
#define H_opengl                       0x0000065312ef9eea
#define H_solid_above                  0xc0d1c0679690db0c
#define H_solid_below                  0xc0d1c06796a48b18
#define H_solid_left                   0x7272f650b702b58a
#define H_solid_right                  0xc0d1c06797c82e7d
#define H_sprite_sheet_id              0xe2141daae50cd180
#define H_width                        0x0000003110a3b0a5

#ifndef USE_LIBTMX // (cute_tiled.h)
#define H_objectgroup                  0xc0b9d518970be349
#define H_tilelayer                    0x0377d9f70e844fb0
#endif

// Private function prototypes
// ----------------------------------------------------------------------------

static esz_status           create_and_set_render_target(SDL_Texture** target, esz_window_t* window);
static esz_status           draw_scene(esz_window_t* window, esz_core_t* core);
static bool                 get_boolean_property(const uint64_t name_hash, esz_tiled_property_t* properties, int32_t property_count, esz_core_t* core);
static double               get_decimal_property(const uint64_t name_hash, esz_tiled_property_t* properties, int32_t property_count, esz_core_t* core);
static int32_t              get_first_gid(esz_tiled_map_t* tiled_map);
static esz_tiled_layer_t*   get_head_tiled_layer(esz_tiled_map_t* tiled_map);
static esz_tiled_object_t*  get_head_tiled_object(esz_tiled_layer_t* tiled_layer, esz_core_t* core);
static esz_tiled_tileset_t* get_head_tiled_tileset(esz_tiled_map_t* tiled_map);
static int32_t              get_integer_property(const uint64_t name_hash, esz_tiled_property_t* tiled_properties, int32_t property_count, esz_core_t* core);
static const char*          get_tiled_layer_name(esz_tiled_layer_t* tiled_layer);
static int32_t              get_tiled_layer_property_count(esz_tiled_layer_t* tiled_layer);
static int32_t              get_tiled_map_property_count(esz_tiled_map_t* tiled_map);
static const char*          get_tiled_object_name(esz_tiled_object_t* tiled_object);
static int32_t              get_tiled_object_property_count(esz_tiled_object_t* tiled_object);
static const char*          get_tiled_object_type_name(esz_tiled_object_t* tiled_object);
static int32_t              get_tile_height(esz_tiled_map_t* tiled_map);
static int32_t              get_tile_width(esz_tiled_map_t* tiled_map);
static esz_status           init_animated_tiles(esz_core_t* core);
static esz_status           init_background(esz_window_t* window, esz_core_t* core);
static esz_status           init_entities(esz_core_t* core);
static esz_status           init_sprites(esz_window_t* window, esz_core_t* core);
static bool                 is_camera_at_horizontal_boundary(esz_core_t* core);
static bool                 is_tiled_layer_of_type(const esz_tiled_layer_type tiled_type, esz_tiled_layer_t* tiled_layer, esz_core_t* core);
static esz_status           load_background_layer(int32_t index, esz_window_t* window, esz_core_t* core);
static void                 load_property(const uint64_t name_hash, esz_tiled_property_t* properties, int32_t property_count, esz_core_t* core);
static esz_status           load_texture_from_file(const char* file_name, SDL_Texture** texture, esz_window_t* window);
static esz_status           load_texture_from_memory(const unsigned char* buffer, const int length, SDL_Texture** texture, esz_window_t* window);
static esz_tiled_map_t*     load_tiled_map(const char* map_file_name);
static void                 move_camera_to_target(esz_window_t* window, esz_core_t* core);
static void                 poll_events(esz_window_t* window, esz_core_t* core);
static int32_t              remove_gid_flip_bits(int32_t gid);
static esz_status           render_actors(int32_t level, esz_window_t* window, esz_core_t* core);
static esz_status           render_background(esz_window_t* window, esz_core_t* core);
static esz_status           render_background_layer(int32_t index, esz_window_t* window, esz_core_t* core);
static esz_status           render_map(int32_t level, esz_window_t* window, esz_core_t* core);
static esz_status           render_scene(esz_window_t* window, esz_core_t* core);
static double               round_(double number);
static void                 set_camera_boundaries_to_map_size(esz_window_t* window, esz_core_t* core);
static void                 update_bounding_box(esz_entity_t* entity);
static void                 update_entities(esz_window_t* window, esz_core_t* core);

#ifdef USE_LIBTMX
static void                tmxlib_store_property(esz_tiled_property_t* property, void* core);
#endif

// Public functions
// ----------------------------------------------------------------------------

bool esz_bounding_boxes_do_intersect(const esz_aabb_t bb_a, const esz_aabb_t bb_b)
{
    double bb_a_x = bb_b.left - bb_a.right;
    double bb_a_y = bb_b.top  - bb_a.bottom;
    double bb_b_x = bb_a.left - bb_b.right;
    double bb_b_y = bb_a.top  - bb_b.bottom;

    if (0.0 < bb_a_x || 0.0 < bb_a_y)
    {
        return false;
    }

    if (0.0 < bb_b_x || 0.0 < bb_b_y)
    {
        return false;
    }

    return true;
}

void esz_clear_player_state(esz_state state, esz_core_t* core)
{
    esz_actor_t* actor;

    if (!esz_is_map_loaded(core))
    {
        return;
    }

    actor = core->map->entity[core->map->active_player_actor_id].actor;

    CLR_STATE(actor->state, state);
}

esz_status esz_create_window(const char* window_title, esz_window_config_t* config, esz_window_t** window)
{
    esz_status      status = ESZ_OK;
    SDL_DisplayMode display_mode;
    uint32_t        renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;
    int32_t         index;

    const unsigned char* esz_logo;
    const unsigned char  esz_logo_pxdata[228] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x07,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x9d, 0x95, 0x1a, 0xe1, 0x00, 0x00, 0x00,
        0xab, 0x49, 0x44, 0x41, 0x54, 0x18, 0xd3, 0x7d, 0x91, 0xa1, 0x19, 0xc2,
        0x30, 0x10, 0x85, 0x5f, 0xf8, 0xaa, 0x4f, 0x54, 0x44, 0x44, 0x31, 0x42,
        0x27, 0x40, 0x22, 0x11, 0xd9, 0x81, 0x15, 0x3a, 0x02, 0xa2, 0x2b, 0x74,
        0x02, 0x4c, 0x45, 0x24, 0x13, 0x44, 0x22, 0x3b, 0x01, 0x22, 0x02, 0x71,
        0x13, 0x20, 0xae, 0x5c, 0xcb, 0x11, 0x78, 0xf2, 0xcf, 0xbb, 0x97, 0x77,
        0xdf, 0xb9, 0xeb, 0xfe, 0x70, 0x7e, 0xde, 0xf1, 0xa9, 0xb1, 0xed, 0x00,
        0x54, 0xf9, 0x37, 0x14, 0x35, 0xf2, 0x50, 0x6e, 0x0c, 0x31, 0x64, 0xf8,
        0x89, 0xd4, 0x6d, 0xf8, 0x0a, 0x01, 0xe5, 0x00, 0xfc, 0x44, 0x8d, 0xba,
        0x7d, 0x4f, 0x00, 0x4a, 0xe4, 0x14, 0xf8, 0xf4, 0x20, 0xc3, 0xb7, 0xf2,
        0xc7, 0x77, 0x62, 0x64, 0x49, 0x01, 0xb0, 0xd3, 0xec, 0x12, 0xb9, 0x44,
        0xce, 0xd9, 0x0e, 0x08, 0x4f, 0x81, 0x65, 0x59, 0xd9, 0xda, 0xa4, 0x8c,
        0x6d, 0xe7, 0x88, 0xa8, 0x0c, 0xbc, 0x6d, 0x5e, 0x55, 0x0a, 0x9c, 0x67,
        0x5c, 0xa8, 0xd2, 0x45, 0xe4, 0x88, 0x96, 0x6f, 0xa5, 0x4b, 0x9e, 0x01,
        0x2c, 0x03, 0xe2, 0x56, 0x6e, 0x52, 0x8c, 0xdf, 0x55, 0xaf, 0xf6, 0x4b,
        0x7f, 0xae, 0xf6, 0x02, 0x39, 0xaf, 0x64, 0xb2, 0x37, 0xc2, 0xf0, 0x45,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    };
    esz_logo = esz_logo_pxdata;

    *window = (esz_window_t*)calloc(1, sizeof(struct esz_window));
    if (! *window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    (*window)->width          = config->width;
    (*window)->height         = config->height;
    (*window)->logical_width  = config->logical_width;
    (*window)->logical_height = config->logical_height;
    (*window)->vsync_enabled  = config->enable_vsync;

    if (config->enable_fullscreen)
    {
        (*window)->flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    if (0 > SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    if (0 != SDL_GetCurrentDisplayMode(0, &display_mode))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    #ifdef __ANDROID__
    (*window)->flags  = 0;
    (*window)->width  = display_mode.w;
    (*window)->height = display_mode.h;
    #endif

    (*window)->refresh_rate  = display_mode.refresh_rate;

    if (0 == (*window)->refresh_rate)
    {
        (*window)->refresh_rate = 60;
        if ((*window)->vsync_enabled)
        {
            SDL_Log("Couldn't determine the monitor's refresh rate: VSync disabled.\n");
            (*window)->vsync_enabled = false;
        }
    }

    if ((*window)->vsync_enabled)
    {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    (*window)->window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        (*window)->width,
        (*window)->height,
        (*window)->flags);

    if (! (*window)->window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    SDL_GetWindowSize((*window)->window, &(*window)->width, &(*window)->height);

    (*window)->zoom_level         = (double)(*window)->height / (double)(*window)->logical_height;
    (*window)->initial_zoom_level = (*window)->zoom_level;

    /* Get index of opengl rendering driver and create 2D rendering
     * context.
     */
    for (index = 0; index < SDL_GetNumRenderDrivers(); index += 1)
    {
        SDL_RendererInfo renderer_info = { 0 };
        SDL_GetRenderDriverInfo(index, &renderer_info);

        if (H_opengl != esz_hash((const unsigned char*)renderer_info.name))
        {
            continue;
        }
        else
        {
            (*window)->renderer = SDL_CreateRenderer((*window)->window, index, renderer_flags);

            if (! (*window)->renderer)
            {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
                status = ESZ_ERROR_CRITICAL;
                goto exit;
            }
            break;
        }
    }

    /* Use default rendering driver if opengl isn't available.
     */
    if (! (*window)->renderer)
    {
        SDL_RendererInfo renderer_info = { 0 };
        SDL_GetRenderDriverInfo(0, &renderer_info);

        SDL_Log("opengl not found: use default rendering driver: %s.\n", renderer_info.name);

        (*window)->renderer = SDL_CreateRenderer((*window)->window, -1, renderer_flags);

        if (! (*window)->renderer)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            status = ESZ_ERROR_CRITICAL;
            goto exit;
        }
    }

    SDL_Log(
        "Setting up window at resolution %dx%d @ %d Hz.\n",
        (*window)->width,
        (*window)->height,
        (*window)->refresh_rate);

    esz_set_zoom_level((*window)->zoom_level, *window);
    SDL_Log("Set initial zoom-level to factor %f.\n", (*window)->zoom_level);

    if (ESZ_OK != load_texture_from_memory(esz_logo, 228, &(*window)->esz_logo, *window))
    {
        status = ESZ_ERROR_CRITICAL;
    }

exit:
    return status;
}

void esz_deactivate_core(esz_core_t* core)
{
    core->is_active = false;
}

void esz_destroy_core(esz_core_t* core)
{
    free(core);
    SDL_Log("Destroy engine core.\n");
}

void esz_destroy_window(esz_window_t* window)
{
    if (window->esz_logo)
    {
        SDL_DestroyTexture(window->esz_logo);
    }

    if (window->renderer)
    {
        SDL_DestroyRenderer(window->renderer);
    }

    if (window->window)
    {
        SDL_DestroyWindow(window->window);
    }

    free(window);
    SDL_Log("Quitting.\n");

    SDL_Quit();
}

bool esz_get_boolean_map_property(const uint64_t name_hash, esz_core_t* core)
{
    int32_t prop_cnt = get_tiled_map_property_count(core->map->handle);

    core->map->boolean_property = false;
    load_property(name_hash, core->map->handle->properties, prop_cnt, core);
    return core->map->boolean_property;
}

double esz_get_decimal_map_property(const uint64_t name_hash, esz_core_t* core)
{
    int32_t prop_cnt = get_tiled_map_property_count(core->map->handle);

    core->map->decimal_property = 0.0;
    load_property(name_hash, core->map->handle->properties, prop_cnt, core);
    return core->map->decimal_property;
}

int32_t esz_get_integer_map_property(const uint64_t name_hash, esz_core_t* core)
{
    int32_t prop_cnt = get_tiled_map_property_count(core->map->handle);

    core->map->integer_property = 0;
    load_property(name_hash, core->map->handle->properties, prop_cnt, core);
    return core->map->integer_property;
}

const char* esz_get_string_map_property(const uint64_t name_hash, esz_core_t* core)
{
    int32_t prop_cnt = get_tiled_map_property_count(core->map->handle);

    core->map->string_property = NULL;
    load_property(name_hash, core->map->handle->properties, prop_cnt, core);
    return core->map->string_property;
}

const uint8_t* esz_get_keyboard_state(void)
{
    return SDL_GetKeyboardState(NULL);
}

uint64_t esz_hash(const unsigned char* name)
{
    uint64_t hash = 5381;
    uint32_t c;

    while ((c = *name++))
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

int32_t esz_get_keycode(esz_core_t* core)
{
    return core->event.handle.key.keysym.sym;
}

double esz_get_time_since_last_frame(esz_window_t* window)
{
    return window->time_since_last_frame;
}

esz_status esz_init_core(esz_core_t** core)
{
    *core = (esz_core_t*)calloc(1, sizeof(struct esz_core));
    if (! *core)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
        return ESZ_ERROR_CRITICAL;
    }

    (*core)->is_active = true;

    return ESZ_OK;
}

bool esz_is_camera_locked(esz_core_t* core)
{
    return core->camera.is_locked;
}

bool esz_is_core_active(esz_core_t* core)
{
    return core->is_active;
}

bool esz_is_map_loaded(esz_core_t* core)
{
    if (core->is_map_loaded)
    {
        return true;
    }

    return false;
}

bool esz_is_player_moving(esz_core_t* core)
{
    esz_actor_t* actor;

    if (! esz_is_map_loaded(core))
    {
        return false;
    }

    actor = core->map->entity[core->map->active_player_actor_id].actor;

    if (IS_STATE_SET(actor->state, STATE_MOVING))
    {
        return true;
    }

    return false;
}

esz_status esz_load_map(const char* map_file_name, esz_window_t* window, esz_core_t* core)
{
    char* tileset_image_source = NULL;

    if (esz_is_map_loaded(core))
    {
        SDL_Log("A map has already been loaded: unload map first.\n");
        return ESZ_WARNING;
    }

    // Load map file and allocate required memory
    // ------------------------------------------------------------------------

    // 1. Map
    // ------------------------------------------------------------------------

    core->map = (esz_map_t*)calloc(1, sizeof(struct esz_map));
    if (! core->map)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
        return ESZ_WARNING;
    }

    core->is_map_loaded = true;

    // 2. Tiled map
    // ------------------------------------------------------------------------

    core->map->handle = load_tiled_map(map_file_name);
    if (! core->map->handle)
    {
        goto warning;
    }

    #ifndef USE_LIBTMX // (cute_tiled.h)
    {
        esz_tiled_layer_t* layer = get_head_tiled_layer(core->map->handle);
        while (layer)
        {
            if (H_tilelayer == esz_hash((const unsigned char*)layer->type.ptr) && ! core->map->hash_id_tilelayer)
            {
                core->map->hash_id_tilelayer = layer->type.hash_id;
                SDL_Log("Set hash ID for tile layer: %llu\n", core->map->hash_id_tilelayer);
            }
            else if (H_objectgroup == esz_hash((const unsigned char*)layer->type.ptr) && ! core->map->hash_id_objectgroup)
            {
                core->map->hash_id_objectgroup = layer->type.hash_id;
                SDL_Log("Set hash ID for object group: %llu\n", core->map->hash_id_objectgroup);
            }
            layer = layer->next;
        }
    }
    #endif

    // 3. Tile properties
    // ------------------------------------------------------------------------

    {
        int32_t tile_count = (int32_t)(core->map->handle->height * core->map->handle->width);

        core->map->tile_properties = (uint32_t*)calloc((size_t)tile_count, sizeof(uint32_t));
        if (!core->map->tile_properties)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
            goto warning;
        }

        {
            esz_tiled_layer_t* layer = get_head_tiled_layer(core->map->handle);

            while (layer)
            {
                if (is_tiled_layer_of_type(ESZ_TILE_LAYER, layer, core))
                {
                    int32_t index_height;
                    for (index_height = 0; index_height < (int32_t)core->map->handle->height; index_height += 1)
                    {
                        int32_t index_width;
                        for (index_width = 0; index_width < (int32_t)core->map->handle->width; index_width += 1)
                        {
                            #ifdef USE_LIBTMX
                            uint32_t* layer_content = layer->content.gids;
                            #else // (cute_tiled.h)
                            int32_t* layer_content = layer->data;
                            #endif
                            int32_t gid = remove_gid_flip_bits((int32_t)layer_content[(index_height * (int32_t)core->map->handle->width) + index_width]);
                            esz_tiled_tileset_t*          tileset = get_head_tiled_tileset(core->map->handle);
                            esz_tiled_tile_t*             tile;

                            #ifdef USE_LIBTMX
                            // tbd.
                            (void)gid;
                            (void)tileset;
                            (void)tile;

                            #else // (cute_tiled.h)
                            tile = tileset->tiles;
                            if (gid)
                            {
                                while (tile)
                                {
                                    if (tile->tile_index == gid)
                                    {
                                        if (0 < tile->property_count)
                                        {
                                            int32_t index = (index_width + 1) * (index_height + 1);

                                            if (get_boolean_property(H_climbable, tile->properties, tile->property_count, core))
                                            {
                                                SET_STATE(core->map->tile_properties[index], TILE_CLIMBABLE);
                                            }

                                            if (get_boolean_property(H_solid_above, tile->properties, tile->property_count, core))
                                            {
                                                SET_STATE(core->map->tile_properties[index], TILE_SOLID_ABOVE);
                                            }
                                            if (get_boolean_property(H_solid_below, tile->properties, tile->property_count, core))
                                            {
                                                SET_STATE(core->map->tile_properties[index], TILE_SOLID_BELOW);
                                            }

                                            if (get_boolean_property(H_solid_left, tile->properties, tile->property_count, core))
                                            {
                                                SET_STATE(core->map->tile_properties[index], TILE_SOLID_LEFT);
                                            }
                                            if (get_boolean_property(H_solid_right, tile->properties, tile->property_count, core))
                                            {
                                                SET_STATE(core->map->tile_properties[index], TILE_SOLID_RIGHT);
                                            }
                                        }
                                        break;
                                    }
                                    tile = tile->next;
                                }
                            }
                            #endif
                        }
                    }
                }
                layer = layer->next;
            }
        }
    }

    // 4. Paths and file locations
    // ------------------------------------------------------------------------

    core->map->path = (char*)calloc(1, (size_t)(SDL_strlen(map_file_name) + 1));
    if (! core->map->path)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
        goto warning;
    }

    cwk_path_get_dirname(map_file_name, &core->map->path_length);
    SDL_strlcpy(core->map->path, map_file_name, core->map->path_length + 1);

    {
        size_t source_length;

        #ifdef USE_LIBTMX
        int32_t first_gid = (int32_t)core->map->handle->ts_head->firstgid;
        char*   ts_path;
        size_t  ts_path_length;

        cwk_path_get_dirname(core->map->handle->ts_head->source, &ts_path_length);

        ts_path = calloc(1, ts_path_length + 1);
        if (! ts_path)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
            goto warning;
        }

        source_length  = SDL_strlen(core->map->path);
        source_length += SDL_strlen(core->map->handle->tiles[first_gid]->tileset->image->source);
        source_length += ts_path_length + 1;

        tileset_image_source = calloc(1, source_length);
        if (! tileset_image_source)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
            free(ts_path);
            goto warning;
        }

        /* The tileset image source is stored relatively to the tileset
         * file but because we only know the location of the tileset
         * file relatively to the map file, we need to adjust the path
         * accordingly.  It's a hack, but it works.
         */

        SDL_strlcpy(ts_path, core->map->handle->ts_head->source, ts_path_length + 1);
        SDL_snprintf(tileset_image_source, source_length, "%s%s%s",
                     core->map->path,
                     ts_path,
                     core->map->handle->tiles[first_gid]->tileset->image->source);

        free(ts_path);

        #else // (cute_tiled.h)
        if (! core->map->handle->tilesets)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: no embedded tileset found.\n", __func__);
            goto warning;
        }

        source_length  = SDL_strlen(core->map->path);
        source_length += SDL_strlen(core->map->handle->tilesets->image.ptr);
        source_length += 1;

        tileset_image_source = (char*)calloc(1, source_length);
        if (! tileset_image_source)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
            esz_unload_map(window, core);
            return ESZ_WARNING;
        }

        SDL_snprintf(tileset_image_source, source_length, "%s%s",
                     core->map->path,
                     core->map->handle->tilesets->image.ptr);

        #endif
    }

    // 5. Entities
    // ------------------------------------------------------------------------

    if (ESZ_OK != init_entities(core))
    {
        free(tileset_image_source);
        goto warning;
    }

    // 6. Tileset
    // ------------------------------------------------------------------------

    if (tileset_image_source)
    {
        if (ESZ_OK != load_texture_from_file(tileset_image_source, &core->map->tileset_texture, window))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: Error loading image '%s'.\n", __func__, tileset_image_source);
            free(tileset_image_source);
            goto warning;
        }
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: Could not determine location of tileset image.\n", __func__);
        goto warning;
    }

    free(tileset_image_source);

    // 7. Sprites
    // ------------------------------------------------------------------------

    if (ESZ_OK != init_sprites(window, core))
    {
        goto warning;
    }

    // 8. Animated tiles
    // ------------------------------------------------------------------------

    if (ESZ_OK != init_animated_tiles(core))
    {
        goto warning;
    }

    // 9. Background
    // ------------------------------------------------------------------------

    if (ESZ_OK != init_background(window, core))
    {
        goto warning;
    }

    SDL_Log(
        "Set gravitational constant to %f (g*%dpx/s^2).\n",
        core->map->gravitation, core->map->meter_in_pixel);

    core->map->animated_tile_fps = esz_get_integer_map_property(H_animated_tile_fps, core);
    if (core->map->animated_tile_fps > window->refresh_rate)
    {
        // It can't update faster anyway.
        core->map->animated_tile_fps = window->refresh_rate;
    }

    if (core->event.map_loaded_cb)
    {
        core->event.map_loaded_cb(window, core);
    }

    // ------------------------------------------------------------------------

    core->map->height = (int32_t)((int32_t)core->map->handle->height * get_tile_height(core->map->handle));
    core->map->width = (int32_t)((int32_t)core->map->handle->width * get_tile_width(core->map->handle));
    core->map->gravitation = esz_get_decimal_map_property(H_gravitation, core);
    core->map->meter_in_pixel = esz_get_integer_map_property(H_meter_in_pixel, core);

    SDL_Log(
        "Load map file: %s containing %d entities(s).\n",
        map_file_name, core->map->entity_count);

    return ESZ_OK;
warning:
    esz_unload_map(window, core);
    return ESZ_WARNING;
}

void esz_lock_camera(esz_core_t* core)
{
    core->camera.is_locked = true;
}

void esz_register_event_callback(const esz_event_type event_type, esz_event_callback event_callback, esz_core_t* core)
{
    switch (event_type)
    {
        case EVENT_FINGERDOWN:
            core->event.finger_down_cb   = event_callback;
            break;
        case EVENT_FINGERUP:
            core->event.finger_up_cb     = event_callback;
            break;
        case EVENT_FINGERMOTION:
            core->event.finger_motion_cb = event_callback;
            break;
        case EVENT_KEYDOWN:
            core->event.key_down_cb      = event_callback;
            break;
        case EVENT_KEYUP:
            core->event.key_up_cb        = event_callback;
            break;
        case EVENT_MAP_LOADED:
            core->event.map_loaded_cb    = event_callback;
            break;
        case EVENT_MAP_UNLOADED:
            core->event.map_unloaded_cb  = event_callback;
            break;
        case EVENT_MULTIGESTURE:
            core->event.multi_gesture_cb = event_callback;
            break;
    }
}

void esz_set_active_player_actor(int32_t id, esz_core_t* core)
{
    if (! esz_is_map_loaded(core))
    {
        return;
    }

    core->map->active_player_actor_id = id;
}

void esz_set_next_player_animation(esz_core_t* core)
{
    esz_actor_t* actor;
    int32_t       id;

    if (! esz_is_map_loaded(core))
    {
        return;
    }

    actor = core->map->entity[core->map->active_player_actor_id].actor;
    id    = actor->current_animation + 1;

    if (id > actor->animation_count)
    {
        id = 1;
    }

    esz_set_player_animation(id, core);
}

void esz_set_player_animation(int32_t id, esz_core_t* core)
{
    esz_actor_t* actor;

    if (!esz_is_map_loaded(core))
    {
        return;
    }

    actor = core->map->entity[core->map->active_player_actor_id].actor;

    if (0 >= id || actor->animation_count < id)
    {
        return;
    }

    if (actor->current_animation != id)
    {
        actor->current_frame     = 0;
        actor->current_animation = id;
    }
}

void esz_set_player_state(esz_state state, esz_core_t* core)
{
    esz_actor_t* actor;

    if (!esz_is_map_loaded(core))
    {
        return;
    }

    actor = core->map->entity[core->map->active_player_actor_id].actor;

    SET_STATE(actor->state, state);
}

void esz_set_camera_position(const double pos_x, const double pos_y, bool pos_is_relative, esz_window_t* window, esz_core_t* core)
{
    if (! esz_is_camera_locked(core))
    {
        if (pos_is_relative)
        {
            double time_factor = (window->time_since_last_frame * 1000.0);

            core->camera.pos_x += pos_x * time_factor;
            core->camera.pos_y += pos_y * time_factor;
        }
        else
        {
            core->camera.pos_x = pos_x;
            core->camera.pos_y = pos_y;
        }

        set_camera_boundaries_to_map_size(window, core);
    }
}

void esz_set_camera_target(const int32_t id, esz_core_t* core)
{
    core->camera.target_actor_id = id;
}

esz_status esz_set_zoom_level(const double factor, esz_window_t* window)
{
    window->zoom_level     = factor;
    window->logical_width  = (int32_t)((double)window->width  / factor);
    window->logical_height = (int32_t)((double)window->height / factor);

    if (0 > SDL_RenderSetLogicalSize(window->renderer, window->logical_width, window->logical_height))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_WARNING;
    }
    else
    {
        return ESZ_OK;
    }
}

esz_status esz_show_scene(esz_window_t* window, esz_core_t* core)
{
    esz_status status;

    status = render_scene(window, core);
    if (ESZ_OK != status)
    {
        goto exit;
    }

    status = draw_scene(window, core);

exit:
    return status;
}

esz_status esz_toggle_fullscreen(esz_window_t* window)
{
    esz_status status = ESZ_OK;

    window->flags = SDL_GetWindowFlags(window->window);

    if (window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
    {
        if (0 > SDL_SetWindowFullscreen(window->window, 0))
        {
            status = ESZ_WARNING;
        }
        SDL_SetWindowPosition(window->window, window->pos_x, window->pos_y);
    }
    else
    {
        SDL_GetWindowPosition(window->window, &window->pos_x, &window->pos_y);

        if (0 > SDL_SetWindowFullscreen(window->window, SDL_WINDOW_FULLSCREEN_DESKTOP))
        {
            status = ESZ_WARNING;
        }
    }

    if (ESZ_OK != status)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
    }

    return status;
}

void esz_trigger_player_action(esz_action action, esz_core_t* core)
{
    esz_actor_t* actor;

    if (!esz_is_map_loaded(core))
    {
        return;
    }

    actor = core->map->entity[core->map->active_player_actor_id].actor;

    SET_STATE(actor->action, action);
}

void esz_unload_map(esz_window_t* window, esz_core_t* core)
{
    int32_t             index;
    esz_tiled_layer_t*  layer;
    esz_tiled_object_t* tiled_object = NULL;

    if (! esz_is_map_loaded(core))
    {
        SDL_Log("No map has been loaded.\n");
        return;
    }
    core->is_map_loaded           = false;
    core->camera.target_actor_id = 0;

    for (index = 0; index < ESZ_MAP_LAYER_LEVEL_MAX; index += 1)
    {
        if (core->map->layer_texture[index])
        {
            SDL_DestroyTexture(core->map->layer_texture[index]);
            core->map->layer_texture[index] = NULL;
        }
    }

    for (index = 0; index < ESZ_RENDER_LAYER_MAX; index += 1)
    {
        if (core->map->render_target[index])
        {
            SDL_DestroyTexture(core->map->render_target[index]);
            core->map->render_target[index] = NULL;
        }
    }

    #ifdef USE_LIBTMX
    if (core->map->animated_tile_texture)
    {
        SDL_DestroyTexture(core->map->animated_tile_texture);
        core->map->animated_tile_texture = NULL;
    }
    #endif

    // Free up allocated memory in reverse order
    // ------------------------------------------------------------------------

    // 9. Background
    // ------------------------------------------------------------------------

    if (0 < core->map->background.layer_count)
    {
        for (index = 0; index < core->map->background.layer_count; index += 1)
        {
            if (core->map->background.layer[index].texture)
            {
                SDL_DestroyTexture(core->map->background.layer[index].texture);
                core->map->background.layer[index].texture = NULL;
            }
        }
    }

    free(core->map->background.layer);

    // 8. Animated tiles
    // ------------------------------------------------------------------------

    free(core->map->animated_tile);

    // 7. Sprites
    // ------------------------------------------------------------------------

    if (0 < core->map->sprite_sheet_count)
    {
        for (index = 0; index < core->map->sprite_sheet_count; index += 1)
        {
            core->map->sprite[index].id = 0;

            if (core->map->sprite[index].texture)
            {
                SDL_DestroyTexture(core->map->sprite[index].texture);
                core->map->sprite[index].texture = NULL;
            }
        }
    }

    free(core->map->sprite);

    // 6. Tileset
    // ------------------------------------------------------------------------

    if (core->map->tileset_texture)
    {
        SDL_DestroyTexture(core->map->tileset_texture);
        core->map->tileset_texture = NULL;
    }

    // 5. Entities
    // ------------------------------------------------------------------------

    index = 0;
    layer = get_head_tiled_layer(core->map->handle);
    while (layer)
    {
        if (is_tiled_layer_of_type(ESZ_OBJECT_GROUP, layer, core))
        {
            tiled_object = get_head_tiled_object(layer, core);

            while (tiled_object)
            {
                uint64_t      type_hash = esz_hash((const unsigned char*)get_tiled_object_type_name(tiled_object));
                esz_entity_t* entity    = &core->map->entity[index];

                switch (type_hash)
                {
                    case H_actor:
                    {
                        esz_actor_t** actor = &entity->actor;
                        free((*actor)->animation);
                        free((*actor));
                    }
                    break;
                }

                index        += 1;
                tiled_object = tiled_object->next;
            }
        }
        layer = layer->next;
    }
    free(core->map->entity);

    // 4. Paths and file locations
    // ------------------------------------------------------------------------

    free(core->map->path);

    // 3. Tile properties
    // ------------------------------------------------------------------------

    free(core->map->tile_properties);

    // 2. Tiled map
    // ------------------------------------------------------------------------

    #ifdef USE_LIBTMX
    if (core->map->handle)
    {
        tmx_map_free(core->map->handle);
    }

    #else // (cute_tiled.h)
    core->map->hash_id_objectgroup = 0;
    core->map->hash_id_tilelayer   = 0;

    if (core->map->handle)
    {
        cute_tiled_free_map(core->map->handle);
    }
    #endif

    // 1. Map
    // ------------------------------------------------------------------------

    free(core->map);

    if (core->event.map_unloaded_cb)
    {
        core->event.map_unloaded_cb(window, core);
    }

    SDL_Log("Unload map.\n");
}

void esz_unlock_camera(esz_core_t* core)
{
    core->camera.is_locked = false;
}

void esz_update_core(esz_window_t* window, esz_core_t* core)
{
    double delta_time = 0.0;

    poll_events(window, core);

    window->time_b = window->time_a;
    window->time_a = SDL_GetTicks();

    if (window->time_a > window->time_b)
    {
        window->time_a = window->time_b;
    }

    delta_time                     = (double)(window->time_b - window->time_a) / 1000.0;
    window->time_since_last_frame  = 1000.0 / (double)window->refresh_rate - delta_time;

    if (! window->vsync_enabled)
    {
        double delay = SDL_floor(window->time_since_last_frame);
        SDL_Delay((uint32_t)delay);
    }

    window->time_since_last_frame /= 1000.0;

    if (! esz_is_map_loaded(core))
    {
        return;
    }

    move_camera_to_target(window, core);
    update_entities(window, core);
}

// Private functions (for internal use only)
// ----------------------------------------------------------------------------

static esz_status create_and_set_render_target(SDL_Texture** target, esz_window_t* window)
{
    if (! (*target))
    {
        (*target) = SDL_CreateTexture(
            window->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_TARGET,
            window->width,
            window->height);
    }

    if (! (*target))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }
    else
    {
        if (0 > SDL_SetTextureBlendMode((*target), SDL_BLENDMODE_BLEND))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            SDL_DestroyTexture((*target));
            return ESZ_ERROR_CRITICAL;
        }
    }

    if (0 > SDL_SetRenderTarget(window->renderer, (*target)))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        SDL_DestroyTexture((*target));
        return ESZ_ERROR_CRITICAL;
    }

    SDL_RenderClear(window->renderer);

    return ESZ_OK;
}

static esz_status draw_scene(esz_window_t* window, esz_core_t* core)
{
    SDL_Rect dst;
    int32_t  index;

    if (0 > SDL_SetRenderTarget(window->renderer, NULL))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
    }

    if (! esz_is_map_loaded(core))
    {
        // Display logo in the lower-right corder.
        dst.x = (window->logical_width)  - 53;
        dst.y = (window->logical_height) - 19;
        dst.w = 48;
        dst.h = 14;

        SDL_SetRenderDrawColor(window->renderer, 0xa9, 0x20, 0x3e, SDL_ALPHA_OPAQUE);

        if (0 > SDL_RenderCopy(window->renderer, window->esz_logo, NULL, &dst))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }

        SDL_RenderPresent(window->renderer);
        SDL_RenderClear(window->renderer);

        return ESZ_OK;
    }

    dst.x = 0;
    dst.y = 0;
    dst.w = window->width;
    dst.h = window->height;

    for (index = 0; index < ESZ_RENDER_LAYER_MAX; index += 1)
    {
        if (0 > SDL_RenderCopy(window->renderer, core->map->render_target[index], NULL, &dst))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }
    }

    SDL_RenderPresent(window->renderer);

    SDL_RenderClear(window->renderer);
    return ESZ_OK;
}

static bool get_boolean_property(const uint64_t name_hash, esz_tiled_property_t* properties, int32_t property_count, esz_core_t* core)
{
    core->map->boolean_property = false;
    load_property(name_hash, properties, property_count, core);
    return core->map->boolean_property;
}

static double get_decimal_property(const uint64_t name_hash, esz_tiled_property_t* properties, int32_t property_count, esz_core_t* core)
{
    core->map->decimal_property = 0.0;
    load_property(name_hash, properties, property_count, core);
    return core->map->decimal_property;
}

static int32_t get_first_gid(esz_tiled_map_t* tiled_map)
{
    #ifdef USE_LIBTMX
    return (int32_t)tiled_map->ts_head->firstgid;

    #else // (cute_tiled.h)
    return tiled_map->tilesets->firstgid;

    #endif
}

static esz_tiled_layer_t* get_head_tiled_layer(esz_tiled_map_t* tiled_map)
{
#ifdef USE_LIBTMX
    return tiled_map->ly_head;

#else // (cute_tiled.h)
    return tiled_map->layers;

#endif
}

static esz_tiled_object_t* get_head_tiled_object(esz_tiled_layer_t* tiled_layer, esz_core_t* core)
{
    if (is_tiled_layer_of_type(ESZ_OBJECT_GROUP, tiled_layer, core))
    {
        #ifdef USE_LIBTMX
        return tiled_layer->content.objgr->head;

        #else // (cute_tiled.h)
        return tiled_layer->objects;

        #endif
    }

    return NULL;
}

static esz_tiled_tileset_t* get_head_tiled_tileset(esz_tiled_map_t* tiled_map)
{
    #ifdef USE_LIBTMX
    int32_t first_gid = get_first_gid(tiled_map);
    return tiled_map->tiles[first_gid]->tileset;

    #else // (cute_tiled.h)
    return tiled_map->tilesets;

    #endif
}

static int32_t get_integer_property(const uint64_t name_hash, esz_tiled_property_t* tiled_properties, int32_t property_count, esz_core_t* core)
{
    core->map->integer_property = 0;
    load_property(name_hash, tiled_properties, property_count, core);
    return core->map->integer_property;
}

static const char* get_tiled_layer_name(esz_tiled_layer_t* tiled_layer)
{
    #ifdef USE_LIBTMX
    return tiled_layer->name;

    #else // (cute_tiled.h)
    return tiled_layer->name.ptr;

    #endif
}

static int32_t get_tiled_layer_property_count(esz_tiled_layer_t* tiled_layer)
{
    #ifdef USE_LIBTMX
    (void)tiled_layer;
    return 0;

    #else // (cute_tiled.h)
    return tiled_layer->property_count;

    #endif
}

static int32_t get_tiled_map_property_count(esz_tiled_map_t* tiled_map)
{
    #ifdef USE_LIBTMX
    (void)tiled_map;
    return 0;

    #else // (cute_tiled.h)
    return tiled_map->property_count;

    #endif
}

static const char* get_tiled_object_name(esz_tiled_object_t* tiled_object)
{
    #ifdef USE_LIBTMX
    return tiled_object->name;

    #else // (cute_tiled.h)
    return tiled_object->name.ptr;

    #endif
}

static int32_t get_tiled_object_property_count(esz_tiled_object_t* tiled_object)
{
    #ifdef USE_LIBTMX
    (void)tiled_object;
    return 0;

    #else // (cute_tiled.h)
    return tiled_object->property_count;

    #endif
}

static const char* get_tiled_object_type_name(esz_tiled_object_t* tiled_object)
{
    #ifdef USE_LIBTMX
    return tiled_object->type;

    #else // (cute_tiled.h)
    return tiled_object->type.ptr;

    #endif
}

static int32_t get_tile_height(esz_tiled_map_t* tiled_map)
{
    #ifdef USE_LIBTMX
    int32_t first_gid = get_first_gid(tiled_map);
    return (int32_t)tiled_map->tiles[first_gid]->tileset->tile_height;

    #else // (cute_tiled.h)
    return tiled_map->tilesets->tileheight;

    #endif
}

static int32_t get_tile_width(esz_tiled_map_t* tiled_map)
{
    #ifdef USE_LIBTMX
    int32_t first_gid = get_first_gid(tiled_map);
    return (int32_t)tiled_map->tiles[first_gid]->tileset->tile_width;

    #else // (cute_tiled.h)
    return tiled_map->tilesets->tilewidth;

    #endif
}

// Todo: simplify / merge libTMX / cute_tiled integration, see : esz_load_map
static esz_status init_animated_tiles(esz_core_t* core)
{
    esz_tiled_layer_t* layer               = get_head_tiled_layer(core->map->handle);
    int32_t            animated_tile_count = 0;

    while (layer)
    {
        if (is_tiled_layer_of_type(ESZ_TILE_LAYER, layer, core) && layer->visible)
        {
            int32_t index_height;
            for (index_height = 0; index_height < (int32_t)core->map->handle->height; index_height += 1)
            {
                int32_t index_width;
                for (index_width = 0; index_width < (int32_t)core->map->handle->width; index_width += 1)
                {
                    #ifdef USE_LIBTMX
                    uint32_t* layer_content = layer->content.gids;
                    #else // (cute_tiled.h)
                    int32_t*  layer_content = layer->data;
                    #endif
                    int32_t gid = remove_gid_flip_bits((int32_t)layer_content[(index_height * (int32_t)core->map->handle->width) + index_width]);

                    #ifdef USE_LIBTMX
                    if (core->map->handle->tiles[gid])
                    {
                        if (core->map->handle->tiles[gid]->animation)
                        {
                            animated_tile_count += 1;
                        }
                    }

                    #else // (cute_tiled.h)
                    {
                        cute_tiled_tileset_t*         tileset   = core->map->handle->tilesets;
                        cute_tiled_tile_descriptor_t* tile      = tileset->tiles;
                        int32_t                       first_gid = core->map->handle->tilesets->firstgid;
                        int32_t                       local_id  = gid - first_gid;

                        if (gid)
                        {
                            while (tile)
                            {
                                if (tile->tile_index == local_id)
                                {
                                    if (tile->animation)
                                    {
                                        animated_tile_count += 1;
                                        break;
                                    }
                                }
                                tile = tile->next;
                            }
                        }
                    }
                    #endif
                }
            }
        }
        layer = layer->next;
    }

    if (0 >= animated_tile_count)
    {
        return ESZ_OK;
    }
    else
    {
        core->map->animated_tile = (esz_animated_tile_t*)calloc((size_t)animated_tile_count, sizeof(struct esz_animated_tile));
        if (!core->map->animated_tile)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
            return ESZ_ERROR_CRITICAL;
        }
    }

    SDL_Log("Initialise %u animated tile(s).\n", animated_tile_count);
    return ESZ_OK;
}

static esz_status init_background(esz_window_t* window, esz_core_t* core)
{
    char    property_name[21] = { 0 };
    bool    search_is_running = true;
    int32_t index;

    core->map->background.layer_shift = esz_get_decimal_map_property(H_background_layer_shift, core);
    core->map->background.velocity    = esz_get_decimal_map_property(H_background_constant_velocity, core);

    if (0.0 < core->map->background.velocity)
    {
        core->map->background.velocity_is_constant = true;
    }

    if (esz_get_boolean_map_property(H_background_is_top_aligned, core))
    {
        core->map->background.alignment = ESZ_TOP;
    }
    else
    {
        core->map->background.alignment = ESZ_BOT;
    }

    core->map->background.layer_count = 0;
    while (search_is_running)
    {
        SDL_snprintf(property_name, 21, "background_layer_%u", core->map->background.layer_count + 1);

        if (esz_get_string_map_property(esz_hash((const unsigned char*)property_name), core))
        {
            core->map->background.layer_count += 1;
        }
        else
        {
            search_is_running = false;
            break;
        }
    }

    if (0 == core->map->background.layer_count)
    {
        return ESZ_OK;
    }

    core->map->background.layer = (esz_background_layer_t*)calloc((size_t)core->map->background.layer_count, sizeof(struct esz_background_layer));
    if (! core->map->background.layer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
        return ESZ_ERROR_CRITICAL;
    }

    if (0 < core->map->background.layer_count)
    {
        for (index = 0; index < core->map->background.layer_count; index += 1)
        {
            load_background_layer(index, window, core);
        }
    }
    else
    {
        return ESZ_OK;
    }

    SDL_Log("Initialise parallax-scrolling background with %u layers.\n", core->map->background.layer_count);

    return ESZ_OK;
}

static esz_status init_entities(esz_core_t* core)
{
    esz_tiled_layer_t*  layer         = get_head_tiled_layer(core->map->handle);
    esz_tiled_object_t* tiled_object  = NULL;
    int32_t             index         = 0;
    bool                player_found  = false;

    if (core->map->entity_count)
    {
        return ESZ_OK;
    }

    while (layer)
    {
        if (is_tiled_layer_of_type(ESZ_OBJECT_GROUP, layer, core))
        {
            tiled_object = get_head_tiled_object(layer, core);
            while (tiled_object)
            {
                core->map->entity_count += 1;
                tiled_object             = tiled_object->next;
            }
        }
        layer = layer->next;
    }

    if (core->map->entity_count)
    {
        core->map->entity = (esz_entity_t*)calloc((size_t)core->map->entity_count, sizeof(struct esz_entity));
        if (! core->map->entity)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
            return ESZ_ERROR_CRITICAL;
        }
    }

    SDL_Log("Initialise %u entities:\n", core->map->entity_count);

    layer = get_head_tiled_layer(core->map->handle);
    while (layer)
    {
        if (is_tiled_layer_of_type(ESZ_OBJECT_GROUP, layer, core))
        {
            tiled_object = get_head_tiled_object(layer, core);
            while (tiled_object)
            {
                uint64_t              type_hash  = esz_hash((const unsigned char*)get_tiled_object_type_name(tiled_object));
                esz_entity_t*         entity     = &core->map->entity[index];
                esz_tiled_property_t* properties = tiled_object->properties;
                int32_t               prop_cnt   = get_tiled_object_property_count(tiled_object);

                entity->pos_x = (double)tiled_object->x;
                entity->pos_y = (double)tiled_object->y;

                switch (type_hash)
                {
                    case H_actor:
                    {
                        esz_actor_t** actor = &entity->actor;

                        (*actor) = (esz_actor_t*)calloc(1, sizeof(struct esz_actor));
                        if (! (*actor))
                        {
                            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory for actor.\n", __func__);
                            return ESZ_ERROR_CRITICAL;
                        }

                        (*actor)->current_animation           = 1;

                        (*actor)->acceleration                = get_decimal_property(H_acceleration, properties, prop_cnt, core);
                        (*actor)->jumping_power               = get_decimal_property(H_jumping_power, properties, prop_cnt, core);
                        (*actor)->max_velocity_x              = get_decimal_property(H_max_velocity_x, properties, prop_cnt, core);

                        (*actor)->sprite_sheet_id             = get_integer_property(H_sprite_sheet_id, properties, prop_cnt, core);

                        (*actor)->connect_horizontal_map_ends = get_boolean_property(H_connect_horizontal_map_ends, properties, prop_cnt, core);
                        (*actor)->connect_vertical_map_ends   = get_boolean_property(H_connect_vertical_map_ends, properties, prop_cnt, core);

                        (*actor)->spawn_pos_x                 = core->map->entity[index].pos_x;
                        (*actor)->spawn_pos_y                 = core->map->entity[index].pos_y;

                        if (get_boolean_property(H_is_affected_by_gravity, properties, prop_cnt, core))
                        {
                            SET_STATE((*actor)->state, STATE_GRAVITATIONAL);
                        }
                        else
                        {
                            SET_STATE((*actor)->state, STATE_FLOATING);
                        }

                        if (get_boolean_property(H_is_animated, properties, prop_cnt, core))
                        {
                            SET_STATE((*actor)->state, STATE_ANIMATED);
                        }

                        if (get_boolean_property(H_is_in_midground, properties, prop_cnt, core))
                        {
                            SET_STATE((*actor)->state, STATE_IN_MIDGROUND);
                        }
                        else if (get_boolean_property(H_is_in_background, properties, prop_cnt, core))
                        {
                            SET_STATE((*actor)->state, STATE_IN_BACKGROUND);
                        }
                        else
                        {
                            SET_STATE((*actor)->state, STATE_IN_FOREGROUND);
                        }

                        if (get_boolean_property(H_is_left_oriented, properties, prop_cnt, core))
                        {
                            SET_STATE((*actor)->state, STATE_GOING_LEFT);
                            SET_STATE((*actor)->state, STATE_LOOKING_LEFT);
                        }
                        else
                        {
                            SET_STATE((*actor)->state, STATE_GOING_RIGHT);
                            SET_STATE((*actor)->state, STATE_LOOKING_RIGHT);
                        }

                        if (get_boolean_property(H_is_moving, properties, prop_cnt, core))
                        {
                            SET_STATE((*actor)->state, STATE_MOVING);
                        }

                        if (get_boolean_property(H_is_player, properties, prop_cnt, core) && ! player_found)
                        {
                            player_found = true;
                            esz_lock_camera(core);
                            esz_set_active_player_actor(index, core);
                            esz_set_camera_target(index, core);

                            SDL_Log("  %d %s *\n", index, get_tiled_object_name(tiled_object));
                        }
                        else
                        {
                            SDL_Log("  %d %s\n", index, get_tiled_object_name(tiled_object));
                        }

                        if (IS_STATE_SET((*actor)->state, STATE_ANIMATED))
                        {
                            char property_name[14] = { 0 };
                            bool search_is_running = true;

                            (*actor)->animation_count = 0;
                            while (search_is_running)
                            {
                                SDL_snprintf(property_name, 14, "animation_%u", (*actor)->animation_count + 1);

                                if (get_boolean_property(esz_hash((const unsigned char*)property_name), properties, prop_cnt, core))
                                {
                                    (*actor)->animation_count += 1;
                                }
                                else
                                {
                                    search_is_running = false;
                                    break;
                                }
                            }
                        }

                        if (0 < (*actor)->animation_count)
                        {
                            char property_name[26] = { 0 };

                            (*actor)->animation = (esz_animation_t*)calloc((size_t)(*actor)->animation_count, sizeof(struct esz_animation));
                            if (! (*actor)->animation)
                            {
                                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
                                return ESZ_ERROR_CRITICAL;
                            }

                            for (index = 0; index < (*actor)->animation_count; index += 1)
                            {
                                SDL_snprintf(property_name, 26, "animation_%u_first_frame", index + 1);
                                (*actor)->animation[index].first_frame =
                                    get_integer_property(esz_hash((const unsigned char*)property_name), properties, prop_cnt, core);

                                if (0 == (*actor)->animation[index].first_frame)
                                {
                                    (*actor)->animation[index].first_frame = 1;
                                }

                                SDL_snprintf(property_name, 26, "animation_%u_fps", index + 1);
                                (*actor)->animation[index].fps =
                                    get_integer_property(esz_hash((const unsigned char*)property_name), properties, prop_cnt, core);

                                SDL_snprintf(property_name, 26, "animation_%u_length", index + 1);
                                (*actor)->animation[index].length =
                                    get_integer_property(esz_hash((const unsigned char*)property_name), properties, prop_cnt, core);

                                SDL_snprintf(property_name, 26, "animation_%u_offset_y", index + 1);
                                (*actor)->animation[index].offset_y =
                                    get_integer_property(esz_hash((const unsigned char*)property_name), properties, prop_cnt, core);
                            }
                        }
                    }
                    break;
                }

                entity->width  = get_integer_property(H_width, properties, prop_cnt, core);
                entity->height = get_integer_property(H_height, properties, prop_cnt, core);

                if (0 >= entity->width)
                {
                    entity->width = get_tile_width(core->map->handle);
                }

                if (0 >= entity->height)
                {
                    entity->width = get_tile_height(core->map->handle);
                }

                update_bounding_box(entity);

                index        += 1;
                tiled_object  = tiled_object->next;
            }
        }
        layer = layer->next;
    }

    if (! player_found)
    {
        SDL_Log("  Warning: no player actor found.\n");
    }

    return ESZ_OK;
}

static esz_status init_sprites(esz_window_t* window, esz_core_t* core)
{
    char    property_name[17] = { 0 };
    bool    search_is_running = true;
    int32_t index;

    core->map->sprite_sheet_count = 0;

    while (search_is_running)
    {
        SDL_snprintf(property_name, 17, "sprite_sheet_%u", core->map->sprite_sheet_count + 1);

        if (esz_get_string_map_property(esz_hash((const unsigned char*)property_name), core))
        {
            core->map->sprite_sheet_count += 1;
        }
        else
        {
            search_is_running = false;
        }
    }

    if (0 == core->map->sprite_sheet_count)
    {
        return ESZ_OK;
    }

    core->map->sprite = (esz_sprite_t*)calloc((size_t)core->map->sprite_sheet_count, sizeof(struct esz_sprite));
    if (! core->map->sprite)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
        return ESZ_ERROR_CRITICAL;
    }

    for (index = 0; index < core->map->sprite_sheet_count; index += 1)
    {
        SDL_snprintf(property_name, 17, "sprite_sheet_%u", index + 1);

        const char* file_name = esz_get_string_map_property(esz_hash((const unsigned char*)property_name), core);

        if (file_name)
        {
            size_t source_length            = SDL_strlen(core->map->path) + SDL_strlen(file_name) + 1;
            char* sprite_sheet_image_source = (char*)calloc(1, source_length);
            if (! sprite_sheet_image_source)
            {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
                return ESZ_ERROR_CRITICAL;
            }

            SDL_snprintf(sprite_sheet_image_source, source_length, "%s%s", core->map->path, file_name);

            core->map->sprite[index].id = index + 1;

            if (ESZ_ERROR_CRITICAL == load_texture_from_file(sprite_sheet_image_source, &core->map->sprite[index].texture, window))
            {
                free(sprite_sheet_image_source);
                return ESZ_ERROR_CRITICAL;
            }

            free(sprite_sheet_image_source);
        }
    }

    return ESZ_OK;
}

static bool is_camera_at_horizontal_boundary(esz_core_t* core)
{
    return core->camera.is_at_horizontal_boundary;
}

static bool is_tiled_layer_of_type(const esz_tiled_layer_type tiled_type, esz_tiled_layer_t* tiled_layer, esz_core_t* core)
{
    switch (tiled_type)
    {
        case ESZ_TILE_LAYER:
            #ifdef USE_LIBTMX
            (void)core;

            if (L_LAYER == tiled_layer->type)
            {
                return true;
            }

            #else // (cute_tiled.h)
            if (core->map->hash_id_tilelayer == tiled_layer->type.hash_id)
            {
                return true;
            }

            #endif
            break;
        case ESZ_OBJECT_GROUP:
            #ifdef USE_LIBTMX
            if (L_OBJGR == tiled_layer->type)
            {
                return true;
            }

            #else // (cute_tiled.h)
            if (core->map->hash_id_objectgroup == tiled_layer->type.hash_id)
            {
                return true;
            }

            #endif
            break;
    }

    return false;
}

static esz_status load_background_layer(int32_t index, esz_window_t* window, esz_core_t* core)
{
    esz_status   status        = ESZ_OK;
    SDL_Texture* image_texture = NULL;
    SDL_Rect     dst;
    int32_t      image_width;
    int32_t      image_height;
    double       layer_width_factor;
    char         property_name[21] = { 0 };
    char*        background_layer_image_source;
    size_t       source_length = 0;

    SDL_snprintf(property_name, 21, "background_layer_%u", index + 1);

    const char* file_name = esz_get_string_map_property(esz_hash((const unsigned char*)property_name), core);
    source_length = SDL_strlen(core->map->path) + SDL_strlen(file_name) + 1;

    background_layer_image_source = (char*)calloc(1, source_length);
    if (! background_layer_image_source)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: error allocating memory.\n", __func__);
        return ESZ_ERROR_CRITICAL;
    }

    SDL_snprintf(background_layer_image_source, source_length, "%s%s", core->map->path, file_name);

    if (ESZ_ERROR_CRITICAL == load_texture_from_file(background_layer_image_source, &image_texture, window))
    {
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    free(background_layer_image_source);

    if (0 > SDL_QueryTexture(image_texture, NULL, NULL, &image_width, &image_height))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    layer_width_factor = SDL_ceil((double)window->width / (double)image_width);

    core->map->background.layer[index].width  = image_width * (int32_t)layer_width_factor;
    core->map->background.layer[index].height = image_height;

    core->map->background.layer[index].texture = SDL_CreateTexture(
        window->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        core->map->background.layer[index].width,
        core->map->background.layer[index].height);

    if (! core->map->background.layer[index].texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    if (0 != SDL_SetRenderTarget(window->renderer, core->map->background.layer[index].texture))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

    dst.x = 0;
    int32_t pass;
    for (pass = 0; pass < layer_width_factor; pass += 1)
    {
        dst.y = 0;
        dst.w = image_width;
        dst.h = image_height;

        if (0 > SDL_RenderCopy(window->renderer, image_texture, NULL, &dst))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            status = ESZ_ERROR_CRITICAL;
            goto exit;
        }

        dst.x += image_width;
    }

    if (0 > SDL_SetTextureBlendMode(core->map->background.layer[index].texture, SDL_BLENDMODE_BLEND))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        status = ESZ_ERROR_CRITICAL;
        goto exit;
    }

exit:
    if (image_texture)
    {
        SDL_DestroyTexture(image_texture);
    }

    SDL_Log("Load background layer %d.\n", index + 1);
    return status;
}

static void load_property(const uint64_t name_hash, esz_tiled_property_t* properties, int32_t property_count, esz_core_t* core)
{
    #ifdef USE_LIBTMX
    (void)property_count;
    core->map->hash_query = name_hash;
    tmx_property_foreach(properties, tmxlib_store_property, (void*)core);

    #else // (cute_tiled.h)
    int index = 0;

    for (index = 0; index < property_count; index += 1)
    {
        if (name_hash == esz_hash((const unsigned char*)properties[index].name.ptr))
        {
            break;
        }
    }

    if (properties[index].name.ptr)
    {
        switch (properties[index].type)
        {
            case CUTE_TILED_PROPERTY_COLOR:
            case CUTE_TILED_PROPERTY_FILE:
            case CUTE_TILED_PROPERTY_NONE:
                // tbd.
                break;
            case CUTE_TILED_PROPERTY_INT:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading integer property '%s': %d\n", properties[index].name.ptr, properties[index].data.integer);
                #endif

                core->map->integer_property = properties[index].data.integer;
                break;
            case CUTE_TILED_PROPERTY_BOOL:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading boolean property '%s': %u\n", properties[index].name.ptr, properties[index].data.boolean);
                #endif

                core->map->boolean_property = (bool)properties[index].data.boolean;
                break;
            case CUTE_TILED_PROPERTY_FLOAT:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading decimal property '%s': %f\n", properties[index].name.ptr, (double)properties[index].data.floating);
                #endif

                core->map->decimal_property = (double)properties[index].data.floating;
                break;
            case CUTE_TILED_PROPERTY_STRING:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading string property '%s': %s\n", properties[index].name.ptr, properties[index].data.string.ptr);
                #endif

                core->map->string_property  = properties[index].data.string.ptr;
                break;
        }
    }
    #endif
}

/* Based on
 * https://wiki.libsdl.org/SDL_CreateRGBSurfaceWithFormatFrom#Code_Examples
 */
static esz_status load_texture_from_file(const char* file_name, SDL_Texture** texture, esz_window_t* window)
{
    SDL_Surface*   surface;
    int            width;
    int            height;
    int            orig_format;
    int            req_format = STBI_rgb_alpha;
    int            depth;
    int            pitch;
    uint32_t       pixel_format;
    unsigned char* data;

    if (! file_name)
    {
        return ESZ_WARNING;
    }

    data = stbi_load(file_name, &width, &height, &orig_format, req_format);

    if (NULL == data)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, stbi_failure_reason());
        return ESZ_ERROR_CRITICAL;
    }

    if (STBI_rgb == req_format)
    {
        depth        = 24;
        pitch        = 3 * width; // 3 bytes per pixel * pixels per row
        pixel_format = SDL_PIXELFORMAT_RGB24;
    }
    else
    {
        // STBI_rgb_alpha (RGBA)
        depth        = 32;
        pitch        = 4 * width;
        pixel_format = SDL_PIXELFORMAT_RGBA32;
    }

    surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)data, width, height, depth, pitch, pixel_format);

    if (NULL == surface)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        stbi_image_free(data);
        return ESZ_ERROR_CRITICAL;
    }

    *texture = SDL_CreateTextureFromSurface(window->renderer, surface);

    SDL_FreeSurface(surface);
    stbi_image_free(data);

    SDL_Log("Loading image from file: %s.\n", file_name);
    return ESZ_OK;
}

static esz_status load_texture_from_memory(const unsigned char* buffer, const int length, SDL_Texture** texture, esz_window_t* window)
{
    SDL_Surface*   surface;
    int            width;
    int            height;
    int            orig_format;
    int            req_format = STBI_rgb_alpha;
    int            depth;
    int            pitch;
    uint32_t       pixel_format;
    unsigned char* data;

    if (! buffer)
    {
        return ESZ_WARNING;
    }

    data = stbi_load_from_memory(buffer, length, &width, &height, &orig_format, req_format);

    if (NULL == data)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, stbi_failure_reason());
        return ESZ_ERROR_CRITICAL;
    }

    if (STBI_rgb == req_format)
    {
        depth        = 24;
        pitch        = 3 * width; // 3 bytes per pixel * pixels per row
        pixel_format = SDL_PIXELFORMAT_RGB24;
    }
    else
    {
        // STBI_rgb_alpha (RGBA)
        depth        = 32;
        pitch        = 4 * width;
        pixel_format = SDL_PIXELFORMAT_RGBA32;
    }

    surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)data, width, height, depth, pitch, pixel_format);

    if (NULL == surface)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        stbi_image_free(data);
        return ESZ_ERROR_CRITICAL;
    }

    *texture = SDL_CreateTextureFromSurface(window->renderer, surface);

    SDL_FreeSurface(surface);
    stbi_image_free(data);

    SDL_Log("Loading image from memory.\n");
    return ESZ_OK;
}

static esz_tiled_map_t* load_tiled_map(const char* map_file_name)
{
    esz_tiled_map_t* tiled_map;

    #ifdef USE_LIBTMX
    tiled_map = (esz_tiled_map_t*)tmx_load(map_file_name);
    if (! tiled_map)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, tmx_strerr());
    }

    #else // (cute_tiled.h)
    tiled_map = (esz_tiled_map_t*)cute_tiled_load_map_from_file(map_file_name, NULL);
    if (! tiled_map)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, cute_tiled_error_reason);
    }

    #endif

    return tiled_map;
}

static void move_camera_to_target(esz_window_t* window, esz_core_t* core)
{
    if (esz_is_camera_locked(core))
    {
        esz_entity_t* target = &core->map->entity[core->camera.target_actor_id];

        core->camera.pos_x  = target->pos_x;
        core->camera.pos_x -= (double)window->logical_width  / 2.0;
        core->camera.pos_y  = target->pos_y;
        core->camera.pos_y -= (double)window->logical_height / 2.0;

        if (0 > core->camera.pos_x)
        {
            core->camera.pos_x = 0;
        }

        set_camera_boundaries_to_map_size(window, core);
    }
}

static void poll_events(esz_window_t* window, esz_core_t* core)
{
    while (0 != SDL_PollEvent(&core->event.handle))
    {
        switch (core->event.handle.type)
        {
            case SDL_QUIT:
                esz_deactivate_core(core);
                return;
            case SDL_FINGERDOWN:
                if (core->event.finger_down_cb)
                {
                    core->event.finger_down_cb(window, core);
                }
                break;
            case SDL_FINGERUP:
                if (core->event.finger_up_cb)
                {
                    core->event.finger_up_cb(window, core);
                }
                break;
            case SDL_FINGERMOTION:
                if (core->event.finger_motion_cb)
                {
                    core->event.finger_motion_cb(window, core);
                }
                break;
            case SDL_KEYDOWN:
                if (core->event.key_down_cb)
                {
                    core->event.key_down_cb(window, core);
                }
                break;
            case SDL_KEYUP:
                if (core->event.key_up_cb)
                {
                    core->event.key_up_cb(window, core);
                }
                break;
            case SDL_MULTIGESTURE:
                if (core->event.multi_gesture_cb)
                {
                    core->event.multi_gesture_cb(window, core);
                }
                break;
        }
    }
}

static int32_t remove_gid_flip_bits(int32_t gid)
{
    #ifdef USE_LIBTMX
    return gid & TMX_FLIP_BITS_REMOVAL;

    #else // (cute_tiled.h)
    return cute_tiled_unset_flags(gid);

    #endif
}

static esz_status render_actors(int32_t level, esz_window_t* window, esz_core_t* core)
{
    esz_tiled_layer_t* layer;
    esz_render_layer   render_layer = ESZ_ACTOR_FG;
    int32_t            index        = 0;

    if (! esz_is_map_loaded(core))
    {
        return ESZ_OK;
    }

    layer = get_head_tiled_layer(core->map->handle);

    if (level >= ESZ_ACTOR_LAYER_LEVEL_MAX)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: invalid layer level selected.\n", __func__);
        return ESZ_ERROR_CRITICAL;
    }

    if (ESZ_ACTOR_LAYER_BG == level)
    {
        render_layer = ESZ_ACTOR_BG;
    }
    else if (ESZ_ACTOR_LAYER_MG == level)
    {
        render_layer = ESZ_ACTOR_MG;
    }

    if (ESZ_OK != create_and_set_render_target(&core->map->render_target[render_layer], window))
    {
        return ESZ_ERROR_CRITICAL;
    }

    while (layer)
    {
        if (is_tiled_layer_of_type(ESZ_OBJECT_GROUP, layer, core))
        {
            esz_tiled_object_t* tiled_object = get_head_tiled_object(layer, core);
            while (tiled_object)
            {
                uint64_t      type_hash = esz_hash((const unsigned char*)get_tiled_object_type_name(tiled_object));
                esz_entity_t* object    = &core->map->entity[index];

                switch (type_hash)
                {
                    case H_actor:
                    {
                        esz_actor_t**    actor = &object->actor;
                        double           pos_x  = object->pos_x - core->camera.pos_x;
                        double           pos_y  = object->pos_y - core->camera.pos_y;
                        SDL_RendererFlip flip   = SDL_FLIP_NONE;
                        SDL_Rect         dst    = { 0 };
                        SDL_Rect         src    = { 0 };

                        if (ESZ_ACTOR_LAYER_BG == level && ! IS_STATE_SET((*actor)->state, STATE_IN_BACKGROUND))
                        {
                            break;
                        }

                        if (ESZ_ACTOR_LAYER_MG == level && ! IS_STATE_SET((*actor)->state, STATE_IN_MIDGROUND))
                        {
                            break;
                        }

                        if (ESZ_ACTOR_LAYER_FG == level && ! IS_STATE_SET((*actor)->state, STATE_IN_FOREGROUND))
                        {
                                break;
                        }

                        if (IS_STATE_SET((*actor)->state, STATE_LOOKING_LEFT))
                        {
                            flip = SDL_FLIP_HORIZONTAL;
                        }

                        // Update animation frame
                        // -----------------------------------------------------

                        if (IS_STATE_SET((*actor)->state, STATE_ANIMATED) && (*actor)->animation)
                        {
                            int32_t current_animation = (*actor)->current_animation;

                            (*actor)->time_since_last_anim_frame += window->time_since_last_frame;

                            if ((*actor)->time_since_last_anim_frame >= 1.0 / (double)((*actor)->animation[current_animation - 1].fps))
                            {
                                (*actor)->time_since_last_anim_frame = 0.0;

                                (*actor)->current_frame += 1;

                                if ((*actor)->current_frame >= (*actor)->animation[current_animation - 1].length)
                                {
                                    (*actor)->current_frame = 0;
                                }
                            }

                            src.x  = ((*actor)->animation[current_animation - 1].first_frame - 1) * object->width;
                            src.x += (*actor)->current_frame                                      * object->width;
                            src.y  = (*actor)->animation[current_animation - 1].offset_y          * object->height;
                        }

                        src.w  = object->width;
                        src.h  = object->height;
                        dst.x  = (int32_t)pos_x - (object->width  / 2);
                        dst.y  = (int32_t)pos_y - (object->height / 2);
                        dst.w  = object->width;
                        dst.h  = object->height;

                        if (0 > SDL_RenderCopyEx(window->renderer, core->map->sprite[(*actor)->sprite_sheet_id].texture, &src, &dst, 0, NULL, flip))
                        {
                            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
                            return ESZ_ERROR_CRITICAL;
                        }
                        break;
                    }
                }
                index        += 1;
                tiled_object  = tiled_object->next;
            }
        }
        layer = layer->next;
    }

    return ESZ_OK;
}

static esz_status render_background(esz_window_t* window, esz_core_t* core)
{
    esz_status       status       = ESZ_OK;
    esz_render_layer render_layer = ESZ_BACKGROUND;
    double           factor;

    if (!esz_is_map_loaded(core))
    {
        return ESZ_OK;
    }

    factor = (double)core->map->background.layer_count + 1.0;

    if (! core->map->render_target[render_layer])
    {
        core->map->render_target[render_layer] = SDL_CreateTexture(
            window->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_TARGET,
            window->width,
            window->height);
    }

    if (! core->map->render_target[render_layer])
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    if (is_camera_at_horizontal_boundary(core))
    {
        if (! core->map->background.velocity_is_constant)
        {
            core->map->background.velocity = 0.0;
        }
    }
    else
    {
        core->map->background.velocity = core->map->entity[core->camera.target_actor_id].actor->velocity_x;
    }

    if (! esz_is_camera_locked(core))
    {
        core->map->background.velocity = 0.0;
    }

    int32_t index;
    for (index = 0; index < core->map->background.layer_count; index += 1)
    {
        core->map->background.layer[index].velocity = core->map->background.velocity / factor;
        factor -= core->map->background.layer_shift;

        status = render_background_layer(index, window, core);

        if (ESZ_OK != status)
        {
            break;
        }
    }

    return status;
}

static esz_status render_background_layer(int32_t index, esz_window_t* window, esz_core_t* core)
{
    esz_render_layer render_layer = ESZ_BACKGROUND;
    int32_t          width        = 0;
    SDL_Rect         dst;
    double           pos_x_a;
    double           pos_x_b;

    if (0 > SDL_QueryTexture(core->map->background.layer[index].texture, NULL, NULL, &width, NULL))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    if (core->map->background.layer[index].pos_x < -width)
    {
        core->map->background.layer[index].pos_x = +width;
    }

    if (core->map->background.layer[index].pos_x > +width)
    {
        core->map->background.layer[index].pos_x = -width;
    }

    pos_x_a = core->map->background.layer[index].pos_x;
    if (0 < pos_x_a)
    {
        pos_x_b = pos_x_a - width;
    }
    else
    {
        pos_x_b = pos_x_a + width;
    }

    if (0 < core->map->background.layer[index].velocity)
    {
        if (ESZ_RIGHT == core->map->background.direction)
        {
            core->map->background.layer[index].pos_x -= core->map->background.layer[index].velocity;
        }
        else
        {
            core->map->background.layer[index].pos_x += core->map->background.layer[index].velocity;
        }
    }

    if (ESZ_TOP == core->map->background.alignment)
    {
        dst.y = (int32_t)(core->map->background.layer[index].pos_y - core->camera.pos_y);
    }
    else
    {
        dst.y = (int32_t)(core->map->background.layer[index].pos_y + (window->logical_height - core->map->background.layer[index].height));
    }

    if (0 > SDL_SetRenderTarget(window->renderer, core->map->render_target[render_layer]))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    if (0 == index)
    {
        SDL_SetRenderDrawColor(
            window->renderer,
            (core->map->handle->backgroundcolor >> 16) & 0xFF,
            (core->map->handle->backgroundcolor >> 8)  & 0xFF,
            (core->map->handle->backgroundcolor)       & 0xFF,
            0);

        SDL_RenderClear(window->renderer);
    }

    dst.x = (int32_t)pos_x_a;
    dst.w = width;
    dst.h = core->map->background.layer[index].height;

    if (0 > SDL_RenderCopyEx(window->renderer, core->map->background.layer[index].texture, NULL, &dst, 0, NULL, SDL_FLIP_NONE))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    dst.x = (int32_t)pos_x_b;
    if (0 > SDL_RenderCopyEx(window->renderer, core->map->background.layer[index].texture, NULL, &dst, 0, NULL, SDL_FLIP_NONE))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    return ESZ_OK;
}

// Todo: simplify/merge libTMX/cute_tiled integration, see: esz_load_map
static esz_status render_map(int32_t level, esz_window_t* window, esz_core_t* core)
{
    esz_tiled_layer_t* layer;
    bool               render_animated_tiles = false;
    esz_render_layer   render_layer          = ESZ_MAP_FG;

    if (! esz_is_map_loaded(core))
    {
        return ESZ_OK;
    }

    layer = get_head_tiled_layer(core->map->handle);

    if (level >= ESZ_MAP_LAYER_LEVEL_MAX)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: invalid layer level selected.\n", __func__);
        return ESZ_ERROR_CRITICAL;
    }

    if (ESZ_MAP_LAYER_BG == level)
    {
        render_layer = ESZ_MAP_BG;

        if (0 < core->map->animated_tile_fps)
        {
            render_animated_tiles = true;
        }
    }

    if (ESZ_OK != create_and_set_render_target(&core->map->render_target[render_layer], window))
    {
        return ESZ_ERROR_CRITICAL;
    }

    // Update and render animated tiles.
    core->map->time_since_last_anim_frame += window->time_since_last_frame;

    if (0 < core->map->animated_tile_index &&
        core->map->time_since_last_anim_frame >= 1.0 / (double)(core->map->animated_tile_fps) && render_animated_tiles)
    {
        core->map->time_since_last_anim_frame = 0.0;

        /* Remark: animated tiles are always rendered in the background
         * layer.
         */
        if (! core->map->animated_tile_texture)
        {
            core->map->animated_tile_texture = SDL_CreateTexture(
                window->renderer,
                SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_TARGET,
                (int32_t)(core->map->width),
                (int32_t)(core->map->height));
        }

        if (! core->map->animated_tile_texture)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }

        if (0 > SDL_SetRenderTarget(window->renderer, core->map->animated_tile_texture))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }

        int32_t index;
        for (index = 0; core->map->animated_tile_index > index; index += 1)
        {
            int32_t  gid          = core->map->animated_tile[index].gid;
            int32_t  tile_id;
            int32_t  next_tile_id = 0;
            SDL_Rect dst;
            SDL_Rect src;

            #ifdef USE_LIBTMX
            tmx_tileset* tileset;
            int32_t      first_gid = (int32_t)core->map->handle->ts_head->firstgid;

            tile_id = core->map->animated_tile[index].id + 1;
            tileset = core->map->handle->tiles[first_gid]->tileset;
            src.x   = (int32_t)core->map->handle->tiles[tile_id]->ul_x;
            src.y   = (int32_t)core->map->handle->tiles[tile_id]->ul_y;
            src.w   = dst.w = get_tile_width(core->map->handle);
            src.h   = dst.h = get_tile_height(core->map->handle);
            dst.x           = (int32_t)core->map->animated_tile[index].dst_x;
            dst.y           = (int32_t)core->map->animated_tile[index].dst_y;

            #else // (cute_tiled.h)
            cute_tiled_tileset_t*         tileset = core->map->handle->tilesets;
            cute_tiled_tile_descriptor_t* tile    = tileset->tiles;

            tile_id = core->map->animated_tile[index].id;
            src.w   = dst.w = get_tile_width(core->map->handle);
            src.h   = dst.h = get_tile_height(core->map->handle);
            dst.x           = (int32_t)core->map->animated_tile[index].dst_x;
            dst.y           = (int32_t)core->map->animated_tile[index].dst_y;

            while (tile)
            {
                if (tile->tile_index == gid)
                {
                    src.x = (tile_id % tileset->columns) * get_tile_width(core->map->handle);
                    src.y = (tile_id / tileset->columns) * get_tile_height(core->map->handle);
                    break;
                }
                tile = tile->next;
            }

            #endif

            if (0 > SDL_RenderCopy(window->renderer, core->map->tileset_texture, &src, &dst))
            {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
                return ESZ_ERROR_CRITICAL;
            }

            core->map->animated_tile[index].current_frame += 1;

            if (core->map->animated_tile[index].current_frame >= core->map->animated_tile[index].animation_length)
            {
                core->map->animated_tile[index].current_frame = 0;
            }

            #ifdef USE_LIBTMX
            next_tile_id = (int32_t)core->map->handle->tiles[gid]->animation[core->map->animated_tile[index].current_frame].tile_id;

            #else // (cute_tiled.h)
            tile = tileset->tiles;
            while (tile)
            {
                if (tile->tile_index == gid)
                {
                    next_tile_id = tile->animation[core->map->animated_tile[index].current_frame].tileid;
                    break;
                }
                tile = tile->next;
            }

            #endif

            core->map->animated_tile[index].id = next_tile_id;
        }

        if (0 > SDL_SetRenderTarget(window->renderer, core->map->layer_texture[level]))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }

        if (0 > SDL_SetRenderTarget(window->renderer, core->map->render_target[render_layer]))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }

        if (0 > SDL_SetTextureBlendMode(core->map->animated_tile_texture, SDL_BLENDMODE_BLEND))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }
    }

    // Texture has already been rendered.
    if (core->map->layer_texture[level])
    {
        double   render_pos_x = core->map->pos_x - core->camera.pos_x;
        double   render_pos_y = core->map->pos_y - core->camera.pos_y;
        SDL_Rect dst          = {
            (int32_t)render_pos_x,
            (int32_t)render_pos_y,
            (int32_t)core->map->width,
            (int32_t)core->map->height
        };

        if (0 > SDL_RenderCopyEx(window->renderer, core->map->layer_texture[level], NULL, &dst, 0, NULL, SDL_FLIP_NONE))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
            return ESZ_ERROR_CRITICAL;
        }

        if (render_animated_tiles)
        {
            if (core->map->animated_tile_texture)
            {
                if (0 > SDL_RenderCopyEx(window->renderer, core->map->animated_tile_texture, NULL, &dst, 0, NULL, SDL_FLIP_NONE))
                {
                    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
                    return ESZ_ERROR_CRITICAL;
                }
            }
        }

        return ESZ_OK;
    }

    // Texture does not yet exist. Render it!
    core->map->layer_texture[level] = SDL_CreateTexture(
        window->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        (int32_t)core->map->width,
        (int32_t)core->map->height);

    if (! core->map->layer_texture[level])
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    if (0 > SDL_SetRenderTarget(window->renderer, core->map->layer_texture[level]))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    while (layer)
    {
        SDL_Rect dst;
        SDL_Rect src;

        if (is_tiled_layer_of_type(ESZ_TILE_LAYER, layer, core))
        {
            bool    is_in_foreground  = false;
            bool    is_layer_rendered = false;
            int32_t prop_cnt = get_tiled_layer_property_count(layer);

            is_in_foreground = get_boolean_property(H_is_in_foreground, layer->properties, prop_cnt, core);

            if (ESZ_MAP_LAYER_BG == level && false == is_in_foreground)
            {
                is_layer_rendered = true;
            }
            else if (ESZ_MAP_LAYER_FG == level && is_in_foreground)
            {
                is_layer_rendered = true;
            }

            if (layer->visible && is_layer_rendered)
            {
                int32_t index_height;
                for (index_height = 0; index_height < (int32_t)core->map->handle->height; index_height += 1)
                {
                    int32_t index_width;
                    for (index_width = 0; index_width < (int32_t)core->map->handle->width; index_width += 1)
                    {
                        #ifdef USE_LIBTMX
                        uint32_t* layer_content = layer->content.gids;
                        #else // (cute_tiled.h)
                        int32_t*  layer_content = layer->data;
                        #endif
                        int32_t gid = remove_gid_flip_bits((int32_t)layer_content[(index_height * (int32_t)core->map->handle->width) + index_width]);

                        #ifdef USE_LIBTMX
                        if (core->map->handle->tiles[gid])
                        {
                            int32_t      first_gid = (int32_t)core->map->handle->ts_head->firstgid;
                            tmx_tileset* tileset;

                            tileset = core->map->handle->tiles[first_gid]->tileset;
                            src.x   = (int32_t)core->map->handle->tiles[gid]->ul_x;
                            src.y   = (int32_t)core->map->handle->tiles[gid]->ul_y;
                            src.w   = dst.w = get_tile_width(core->map->handle);
                            src.h   = dst.h = get_tile_height(core->map->handle);
                            dst.x           = (int32_t)(index_width  * get_tile_width(core->map->handle));
                            dst.y           = (int32_t)(index_height * get_tile_height(core->map->handle));

                            SDL_RenderCopy(window->renderer, core->map->tileset_texture, &src, &dst);

                            if (render_animated_tiles && core->map->handle->tiles[gid]->animation)
                            {
                                int32_t animation_length = (int32_t)core->map->handle->tiles[gid]->animation_len;
                                int32_t id               = (int32_t)core->map->handle->tiles[gid]->animation[0].tile_id;

                                core->map->animated_tile[core->map->animated_tile_index].gid              = gid;
                                core->map->animated_tile[core->map->animated_tile_index].id               = id;
                                core->map->animated_tile[core->map->animated_tile_index].dst_x            = dst.x;
                                core->map->animated_tile[core->map->animated_tile_index].dst_y            = dst.y;
                                core->map->animated_tile[core->map->animated_tile_index].current_frame    = 0;
                                core->map->animated_tile[core->map->animated_tile_index].animation_length = animation_length;

                                core->map->animated_tile_index += 1;
                            }
                        }

                        #else // (cute_tiled.h)
                        if (gid)
                        {
                            cute_tiled_tileset_t* tileset;
                            int32_t               first_gid = core->map->handle->tilesets->firstgid;
                            int32_t               local_id  = gid - first_gid;

                            tileset = core->map->handle->tilesets;
                            src.x   = ((int32_t)local_id % tileset->columns) * get_tile_width(core->map->handle);
                            src.y   = ((int32_t)gid / tileset->columns)      * get_tile_height(core->map->handle);
                            src.w   = dst.w = get_tile_width(core->map->handle);
                            src.h   = dst.h = get_tile_height(core->map->handle);
                            dst.x           = (int32_t)(index_width  * get_tile_width(core->map->handle));
                            dst.y           = (int32_t)(index_height * get_tile_height(core->map->handle));

                            SDL_RenderCopy(window->renderer, core->map->tileset_texture, &src, &dst);

                            if (render_animated_tiles)
                            {
                                cute_tiled_tile_descriptor_t* tile = tileset->tiles;

                                while (tile)
                                {
                                    if (tile->tile_index == local_id)
                                    {
                                        if (tile->animation)
                                        {
                                            int32_t animation_length = tile->frame_count;
                                            int32_t id               = tile->animation->tileid;

                                            core->map->animated_tile[core->map->animated_tile_index].gid              = local_id;
                                            core->map->animated_tile[core->map->animated_tile_index].id               = id;
                                            core->map->animated_tile[core->map->animated_tile_index].dst_x            = dst.x;
                                            core->map->animated_tile[core->map->animated_tile_index].dst_y            = dst.y;
                                            core->map->animated_tile[core->map->animated_tile_index].current_frame    = 0;
                                            core->map->animated_tile[core->map->animated_tile_index].animation_length = animation_length;

                                            core->map->animated_tile_index += 1;

                                            break;
                                        }
                                    }
                                    tile = tile->next;
                                }
                            }
                        }
                        #endif
                    }
                }

                {
                    const char* layer_name = get_tiled_layer_name(layer);
                    SDL_Log("Render map layer: %s\n", layer_name);
                }
            }
        }
        layer = layer->next;
    }

    if (0 > SDL_SetRenderTarget(window->renderer, core->map->render_target[render_layer]))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    if (0 > SDL_SetTextureBlendMode(core->map->layer_texture[level], SDL_BLENDMODE_BLEND))
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s: %s.\n", __func__, SDL_GetError());
        return ESZ_ERROR_CRITICAL;
    }

    return ESZ_OK;
}

static esz_status render_scene(esz_window_t* window, esz_core_t* core)
{
    esz_status status = ESZ_OK;
    int32_t    index;

    status = render_background(window, core);
    if (ESZ_OK != status)
    {
        return status;
    }

    for (index = 0; index < ESZ_MAP_LAYER_LEVEL_MAX; index  += 1)
    {
        status = render_map(index, window, core);
        if (ESZ_OK != status)
        {
            return status;
        }
    }

    for (index = 0; index < ESZ_ACTOR_LAYER_LEVEL_MAX; index += 1)
    {
        status = render_actors(index, window, core);
        if (ESZ_OK != status)
        {
            return status;
        }
    }

    return status;
}

static double round_(double number)
{
    double decimal_place = number - SDL_floor(number);

    if (decimal_place >= 0.5)
    {
        return SDL_ceil(number);
    }
    else
    {
        return SDL_floor(number);
    }
}

static void set_camera_boundaries_to_map_size(esz_window_t* window, esz_core_t* core)
{
    core->camera.is_at_horizontal_boundary = false;
    core->camera.max_pos_x                 = (int32_t)core->map->width  - window->logical_width;
    core->camera.max_pos_y                 = (int32_t)core->map->height - window->logical_height;

    if (0 >= core->camera.pos_x)
    {
        core->camera.pos_x                     = 0;
        core->camera.is_at_horizontal_boundary = true;
    }

    if (0 >= core->camera.pos_y)
    {
        core->camera.pos_y = 0;
    }

    if (core->camera.pos_x >= core->camera.max_pos_x)
    {
        core->camera.pos_x                     = core->camera.max_pos_x;
        core->camera.is_at_horizontal_boundary = true;
    }

    if (core->camera.pos_y >= core->camera.max_pos_y)
    {
        core->camera.pos_y = core->camera.max_pos_y;
    }
}

static void update_bounding_box(esz_entity_t* entity)
{
    entity->bounding_box.top    = entity->pos_y - (double)(entity->height / 2.0);
    entity->bounding_box.bottom = entity->pos_y + (double)(entity->height / 2.0);
    entity->bounding_box.left   = entity->pos_x - (double)(entity->width  / 2.0);
    entity->bounding_box.right  = entity->pos_x + (double)(entity->width  / 2.0);

    if (0 >= entity->bounding_box.left)
    {
        entity->bounding_box.left = 0.0;
    }

    if (0 >= entity->bounding_box.top)
    {
        entity->bounding_box.top = 0.0;
    }
}

static void update_entities(esz_window_t* window, esz_core_t* core)
{
    esz_tiled_layer_t* layer;
    int32_t            index = 0;

    if (! esz_is_map_loaded(core))
    {
        return;
    }

    layer = get_head_tiled_layer(core->map->handle);

    while (layer)
    {
        if (is_tiled_layer_of_type(ESZ_OBJECT_GROUP, layer, core))
        {
            esz_tiled_object_t* tiled_object = get_head_tiled_object(layer, core);
            while (tiled_object)
            {
                uint64_t      type_hash = esz_hash((const unsigned char*)get_tiled_object_type_name(tiled_object));
                esz_entity_t* entity    = &core->map->entity[index];

                switch (type_hash)
                {
                    case H_actor:
                    {
                        esz_actor_t** actor = &entity->actor;
                        uint32_t*     state = &(*actor)->state;
                        double        acceleration_x = (*actor)->acceleration * core->map->meter_in_pixel;
                        double        acceleration_y = core->map->meter_in_pixel * core->map->meter_in_pixel;
                        double        time_since_last_frame = esz_get_time_since_last_frame(window);
                        double        distance_x = acceleration_x * time_since_last_frame * time_since_last_frame;
                        double        distance_y = acceleration_y * time_since_last_frame * time_since_last_frame;

                        // Adjust mutually exclusive states
                        // ----------------------------------------------------
                        if (IS_STATE_SET(*state, STATE_IN_MIDGROUND))
                        {
                            CLR_STATE((*actor)->state, STATE_IN_BACKGROUND);
                            CLR_STATE((*actor)->state, STATE_IN_FOREGROUND);
                        }
                        else if (IS_STATE_SET(*state, STATE_IN_FOREGROUND))
                        {
                            CLR_STATE((*actor)->state, STATE_IN_BACKGROUND);
                            CLR_STATE((*actor)->state, STATE_IN_MIDGROUND);
                        }
                        else if (IS_STATE_SET(*state, STATE_IN_BACKGROUND))
                        {
                            CLR_STATE((*actor)->state, STATE_IN_MIDGROUND);
                            CLR_STATE((*actor)->state, STATE_IN_FOREGROUND);
                        }
                        else
                        {
                            SET_STATE((*actor)->state, STATE_IN_MIDGROUND);
                        }

                        // Vertical movement and gravity
                        // ----------------------------------------------------

                        if (IS_STATE_SET(*state, STATE_GRAVITATIONAL))
                        {
                            CLR_STATE((*actor)->state, STATE_FLOATING);

                            if (0 > (*actor)->velocity_y)
                            {
                                SET_STATE((*actor)->state, STATE_RISING);
                            }
                            else
                            {
                                CLR_STATE((*actor)->state, STATE_RISING);
                            }

                            if (IS_STATE_SET((*actor)->state, STATE_RISING))
                            {
                                SET_STATE((*actor)->state, STATE_IN_MID_AIR);
                            }

                            // tbd. check ground collision here
                        }
                        else
                        {
                            SET_STATE((*actor)->state, STATE_FLOATING);
                            CLR_STATE((*actor)->state, STATE_IN_MID_AIR);
                            CLR_STATE((*actor)->state, STATE_JUMPING);
                            CLR_STATE((*actor)->state, STATE_RISING);
                        }

                        if (0 < core->map->gravitation)
                        {
                            if (IS_STATE_SET((*actor)->state, STATE_IN_MID_AIR))
                            {
                                (*actor)->velocity_y += distance_y;
                                entity->pos_y += (*actor)->velocity_y;
                            }
                            else
                            {
                                int32_t tile_height = get_tile_height(core->map->handle);

                                CLR_STATE((*actor)->action, ACTION_JUMP);
                                (*actor)->velocity_y = 0.0;
                                // Correct actor position along the y-axis:
                                entity->pos_y = ((double)tile_height * round(entity->pos_y / (double)tile_height));
                            }
                        }
                        else
                        {
                            if (IS_STATE_SET((*actor)->state, STATE_MOVING))
                            {
                                (*actor)->velocity_y += distance_y;
                            }
                            else
                            {
                                (*actor)->velocity_y -= distance_y;
                            }

                            if (0.0 < (*actor)->velocity_y)
                            {
                                if (IS_STATE_SET((*actor)->state, STATE_GOING_UP))
                                {
                                    entity->pos_y -= (*actor)->velocity_y;
                                }
                                else if (IS_STATE_SET((*actor)->state, STATE_GOING_DOWN))
                                {
                                    entity->pos_y += (*actor)->velocity_y;
                                }
                            }

                            /* Since the velocity in free fall is
                             * normally not limited, the maximum
                             * horizontal velocity is used in this case.
                             */
                            if ((*actor)->max_velocity_x <= (*actor)->velocity_y)
                            {
                                (*actor)->velocity_y = (*actor)->max_velocity_x;
                            }
                            else if (0.0 > (*actor)->velocity_x)
                            {
                                (*actor)->velocity_y = 0.0;
                            }
                        }

                        // Horizontal movement
                        // ----------------------------------------------------

                        if (IS_STATE_SET((*actor)->state, STATE_MOVING))
                        {
                            (*actor)->velocity_x += distance_x;
                        }
                        else
                        {
                            // tbd. friction
                            (*actor)->velocity_x -= distance_x * 2.0;
                        }

                        if (0.0 < (*actor)->velocity_x)
                        {
                            if (IS_STATE_SET((*actor)->state, STATE_GOING_LEFT))
                            {
                                entity->pos_x -= (*actor)->velocity_x;
                            }
                            else if (IS_STATE_SET((*actor)->state, STATE_GOING_RIGHT))
                            {
                                entity->pos_x += (*actor)->velocity_x;
                            }
                        }

                        if ((*actor)->max_velocity_x <= (*actor)->velocity_x)
                        {
                            (*actor)->velocity_x = (*actor)->max_velocity_x;
                        }
                        else if (0.0 > (*actor)->velocity_x)
                        {
                            (*actor)->velocity_x = 0.0;
                        }

                        // Connect map ends
                        // ----------------------------------------------------

                        if ((*actor)->connect_horizontal_map_ends)
                        {
                            if (0.0 - entity->width > entity->pos_x)
                            {
                                entity->pos_x = core->map->width + entity->width;
                            }
                            else if (core->map->width + entity->width < entity->pos_x)
                            {
                                entity->pos_x = 0.0 - entity->width;
                            }
                        }
                        else
                        {
                            if ((double)(entity->width / 4) > entity->pos_x)
                            {
                                entity->pos_x = (double)(entity->width / 4);
                            }
                            // tbd.
                        }

                        if ((*actor)->connect_vertical_map_ends)
                        {
                            if (0.0 - entity->height > entity->pos_y)
                            {
                                entity->pos_y = core->map->height + entity->width;
                            }
                            else if (core->map->height + entity->height < entity->pos_y)
                            {
                                entity->pos_y = 0.0 - entity->height;
                            }
                        }
                        else
                        {
                            // tbd.
                        }

                       break;
                    }
                }

                // Update axis-aligned bounding box
                // ------------------------------------------------------------

                update_bounding_box(entity);

                index        += 1;
                tiled_object  = tiled_object->next;
            }
        }
        layer = layer->next;
    }
}

#ifdef USE_LIBTMX
static void tmxlib_store_property(esz_tiled_property_t* property, void* core)
{
    esz_core_t* core_ptr = core;

    if (core_ptr->map->hash_query == esz_hash((const unsigned char*)property->name))
    {
        switch (property->type)
        {
            case PT_COLOR:
            case PT_NONE:
                // tbd.
                break;
            case PT_BOOL:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading boolean property '%s': %u\n", property->name, property->value.boolean);
                #endif

                core_ptr->map->boolean_property = (bool)property->value.boolean;
                break;
            case PT_FILE:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading string property '%s': %s\n", property->name, property->value.file);
                #endif

                core_ptr->map->string_property  = property->value.file;
                break;
            case PT_FLOAT:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading decimal property '%s': %f\n", property->name, (double)property->value.decimal);
                #endif

                core_ptr->map->decimal_property = (double)property->value.decimal;
                break;
            case PT_INT:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading integer property '%s': %d\n", property->name, property->value.integer);
                #endif

                core_ptr->map->integer_property = property->value.integer;
                break;
            case PT_STRING:
                #ifdef ESZ_DEBUG
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             "Loading string property '%s': %s\n", property->name, property->value.string);
                #endif

                core_ptr->map->string_property  = property->value.string;
                break;
        }
    }
}
#endif
