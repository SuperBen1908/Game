
#define PLATFORM_API_N 10

#ifndef PLATFORM
#ifdef GAME_INTERNAL
void  *(*platform_DEBUG_read_entire_file_f) (char *file_name, u32 *file_size);
void   (*platform_DEBUG_free_file_memory_f) (void *memory);
bool32 (*platform_DEBUG_write_entire_file_f)(char *filename, u32 memory_size, void *memory);
#endif

// always in micro
game_time_t (*platform_get_clock_f)(void);


void game_get_platform(void **platform_api[PLATFORM_API_N])
{
  platform_api[0] = (void **)&platform_DEBUG_read_entire_file_f;
  platform_api[1] = (void **)&platform_DEBUG_free_file_memory_f;
  platform_api[2] = (void **)&platform_DEBUG_write_entire_file_f;
  platform_api[3] = (void **)&platform_get_clock_f;
}
#endif


