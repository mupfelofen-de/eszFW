#define SDL_MAIN_HANDLED

#include <stdbool.h>
#include <stdlib.h>
#include <SDL.h>
#include <esz.h>

static void key_down_callback(esz_window_t* window, esz_core_t* core);

int main()
{
    esz_status          status;
    esz_window_t*       window = NULL;
    esz_window_config_t config = { 640, 360, 384, 216, false, false };
    esz_core_t*         core   = NULL;

    status = esz_create_window("eszFW", &config, &window);
    if (ESZ_OK != status)
    {
        goto quit;
    }

    status = esz_init_core(&core);
    if (ESZ_OK != status)
    {
        goto quit;
    }

    if (ESZ_OK == esz_load_map("res/maps/example.tmx", window, core))
    {
        esz_register_event_callback(EVENT_KEYDOWN, &key_down_callback, core);
    }
    else
    {
        esz_deactivate_core(core);
    }

    while (esz_is_core_active(core))
    {
        esz_update_core(window, core);

        status = esz_show_scene(window, core);
        if (ESZ_ERROR_CRITICAL == status)
        {
            break;
        }
    }

quit:
    if (esz_is_map_loaded(core))
    {
        esz_unload_map(window, core);
    }
    if (core)
    {
        esz_destroy_core(core);
    }
    if (window)
    {
        esz_destroy_window(window);
    }

    if (ESZ_OK != status)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void key_down_callback(esz_window_t* window, esz_core_t* core)
{
    switch (esz_get_keycode(core))
    {
        case SDLK_q:
            esz_deactivate_core(core);
            break;
        case SDLK_F11:
            esz_toggle_fullscreen(window);
            break;
    }
}
