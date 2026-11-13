
#include <stdint.h>

#define TRUE 1
#define FALSE 0
#define PI 3.14159265358979323846F
#define EPSILON 0.0001


#define LOCAL_PERSIST   static 
#define GLOBAL_VARIABLE static 
#define INTERNAL        static

#ifdef RELEASE
#define ASSERT(expr) 
#else
#define ASSERT(expr) if (!(expr)) *(int*)NULL = 0;
#endif

#define arr_size(arr) (sizeof(arr) / sizeof((arr)[0]))


typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  b8;
typedef uint16_t b16;
typedef uint32_t b32;
typedef uint64_t b64;

typedef float    r32;
typedef double   r64;

#define IS_ZERO(n) ((n) < EPSILON && (n) > -EPSILON)

#define KB(n) ((u64)(n)*1024)
#define MB(n) (KB(n)*1024)
#define GB(n) (MB(n)*1024)

#define MSEC2SEC(msec)  ((msec)/1000.0F)
#define USEC2MSEC(usec) ((usec)/1000.0F)
#define USEC2SEC(usec)  ((usec)/1000000.0F)

#define SEC2MSEC(sec)   ((sec)*1000.0F)
#define MSEC2USEC(msec) ((msec)*1000.0F)
#define SEC2USEC(sec)   ((sec)*1000000.0F)

#define RAD2DEG(rad) ((rad)*180.0F/PI)
#define DEG2RAD(deg) ((deg)*PI/180.0F)


// math
typedef union {
  r32 v[2];
  struct { r32 x; r32 y; };
} vec2_t;

typedef union {
  r32 v[3];
  struct { r32 x; r32 y; r32 z; };
} vec3_t;

typedef union {
  r32 v[4];
  struct { r32 x; r32 y; r32 z; r32 w; };
} vec4_t;

typedef union {
  struct { r32 x; r32 y; r32 z; };
  struct { r32 i; r32 j; r32 k; };
  struct { vec3_t v; r32 w; };
} quaternion_t;

typedef union {
  vec2_t v[2];
  r32 m[2][2];
} mat2_t;

typedef union {
  vec3_t v[3];
  r32 m[3][3];
} mat3_t;

typedef union {
  vec4_t v[4];
  r32 m[4][4];
} mat4_t;

typedef struct {
  vec3_t p1;
  vec3_t p2;
  vec3_t p3;
} triangle_t;


// arena
typedef struct {
  u8  *pool;
  u32  top;
  u32  size;
} arena_t;


// png
typedef union {
  u8  channels[4];
  u32 out;
  struct {
    u8 r:8;
    u8 g:8;
    u8 b:8;
    u8 a:8;
  };
} png_pixel_t;

typedef struct {
  u32 width;
  u32 height;
  u32 pixel_bytes;
  u32 pixel_bits;
  u32 stride;

  struct {
    u8  rgb_active:1;
    u8  alpha_active:1;
    u8  greyscale_active:1;
    u8  pallete_active:1;
    u8  sizes_in_bits:1;
  };

  png_pixel_t *pixels;

  arena_t arena;
} png_data_t;


// game
typedef struct {
  void *memory;
  s32   width;
  s32   height;
  s32   bytes_per_pixel;
  s32   pitch;
  s32   bitmap_memory_size;
} game_offscreen_buffer_t;

typedef struct {
  r32  *memory;
  s32   width;
  s32   height;
  s32   pitch;
} game_depth_buffer_t;


typedef struct {
  triangle_t *triangles;
  u32 triangle_count;
} game_sim_region_t;


typedef struct {
  s32  samples_per_sec;
  u32  samples_count;
  s16 *samples;
} game_sound_output_t;


typedef enum {
  KEY_RELEASED,
  KEY_PRESSED,
} game_key_state_e;

typedef enum {
  LOGIC_KEY_RIGHT,
  LOGIC_KEY_LEFT,
  LOGIC_KEY_UP,
  LOGIC_KEY_DOWN,
  LOGIC_KEY_FORWARD,
  LOGIC_KEY_BACKWARD,

  LOGIC_KEY_ROT_UP,
  LOGIC_KEY_ROT_DOWN,
  LOGIC_KEY_ROT_RIGHT,
  LOGIC_KEY_ROT_LEFT,
  LOGIC_KEY_ROT_FORWARD,
  LOGIC_KEY_ROT_BACKWARD,

  LOGIC_KEY_ATTACK,
  LOGIC_KEY_DEFEND,
  LOGIC_KEY_HEAL,
  LOGIC_KEY_QUIT,
  LOGIC_KEYS_N, 
} game_key_e;

typedef struct {
  u8 press_rel;
} game_key_t;

typedef struct {
  game_key_t keys[LOGIC_KEYS_N];
} game_input_t;


typedef struct {
  arena_t global_arena;
  arena_t frame_arena;
  game_offscreen_buffer_t offscreen_buffer;
  game_depth_buffer_t depth_buffer;

  b32 running;
  r32 dt;

  game_input_t prev_input;

  // sound
  r32 volume;
  r32 freq;
  r32 f_step;
  r32 f_inv_step;
  r32 t_step;
  r32 t;
  b32 start;
  b32 start_samp_count;
  b32 start_samp_total;

  // camera
  struct {
    r32 fov;

    vec3_t position;
    vec3_t velocity;
    vec3_t accelaration;

    r32 far;
    r32 near;

    // R U F
    quaternion_t rotation;
    vec3_t       rotation_velocity;
  } camera;

  game_sim_region_t sim_region;

} game_state_t;


typedef struct {
  u8  *memory;
  u64  memory_size;
} game_memory_t;

typedef u64 game_time_t;


