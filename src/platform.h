
#ifdef GAME_INTERNAL
void  *DEBUG_platform_read_entire_file (char *file_name, u32 *file_size);
void   DEBUG_platform_free_file_memory (void *memory);
bool32 DEBUG_platform_write_entire_file(char *filename, u32 memory_size, void *memory);
#endif

// always in micro
inline game_time_t platform_get_clock(void);

