

#define DEFINE_STUB(ret_type, funcname, args, ret_val)         \
          typedef ret_type funcname##_t##args;                 \
          ret_type funcname##args;                             \
          ret_type funcname##_stub_f##args { return ret_val; } \
          GLOBAL_VARIABLE funcname##_t *funcname##_f = funcname##_stub_f;


#define PLATFORM_API_N 10
#define GAME_API_N 10


DEFINE_STUB(b32, platform_write_file, (char *filename, u32 memory_size, void *memory), FALSE)
DEFINE_STUB(game_time_t, platform_get_clock, (void), 0)

#ifdef GAME_INTERNAL
DEFINE_STUB(void *, platform_DEBUG_read_entire_file, (char *file_name, u32 *file_size), NULL)
DEFINE_STUB(void, platform_DEBUG_free_file_memory, (void *memory), )
#endif

DEFINE_STUB(void, game_init, (game_memory_t *game_memory, b32 hard), )
DEFINE_STUB(void, game_get_sound_samples, (game_sound_output_t *sound_output_buffer), )
DEFINE_STUB(void, game_update_and_render, \
            (game_offscreen_buffer_t *offscreen_buffer, game_input_t *input, r32 dt), )


DEFINE_STUB(void, platform_set_game, (void *game_api[GAME_API_N]), )
DEFINE_STUB(void, game_set_platform, (void *platform_api[PLATFORM_API_N]), )


#ifdef PLATFORM
/************/
/* PLATFORM */
/************/
void platform_reload_game(game_set_platform_t *game_set_platform)
{
  void *platform_api[PLATFORM_API_N] = {
    (void *)platform_write_file,
    (void *)platform_get_clock,
#ifdef GAME_INTERNAL
    (void *)platform_DEBUG_read_entire_file,
    (void *)platform_DEBUG_free_file_memory,
#endif
    (void *)platform_set_game,

    // (void *)platform_file_len,
    // (void *)platform_file_read,
  };

  game_set_platform(platform_api);
}

void platform_set_game(void *game_api[GAME_API_N])
{
  u32 i = 0;

  *(void **)&game_init_f              = game_api[i++];
  *(void **)&game_get_sound_samples_f = game_api[i++];
  *(void **)&game_update_and_render_f = game_api[i++];
}

#else
/********/
/* GAME */
/********/
void game_set_platform(void *platform_api[PLATFORM_API_N])
{
  u32 i = 0;

  *(void **)&platform_write_file_f = platform_api[i++];
  *(void **)&platform_get_clock_f  = platform_api[i++];
#ifdef GAME_INTERNAL
  *(void **)&platform_DEBUG_read_entire_file_f = platform_api[i++];
  *(void **)&platform_DEBUG_free_file_memory_f = platform_api[i++];
#endif
  *(void **)&platform_set_game_f = platform_api[i++];

  void *game_api[GAME_API_N] = {
    (void *)game_init,             
    (void *)game_get_sound_samples,
    (void *)game_update_and_render,
  };

  platform_set_game_f(game_api);
}

#endif


