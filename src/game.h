
#include <stdint.h>

#define TRUE 1
#define FALSE 0
#define PI 3.14159265358979323846F
#define EPSILON 0.0001


#define LOCAL_PERSIST   static 
#define GLOBAL_VARIABLE static 
#define INTERNAL        static

#ifdef GAME_INTERNAL
#define ASSERT(expr) if (!(expr)) *(int*)NULL = 0;
#else
#define ASSERT(expr) 
#endif


typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint8_t  bool8;
typedef uint16_t bool16;
typedef uint32_t bool32;
typedef uint64_t bool64;

typedef float    real32;
typedef double   real64;

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


// Generic structures

typedef union vec2_t {
  real32 v[2];
  struct { real32 x; real32 y; };
} vec2_t;

typedef union vec3_t {
  real32 v[3];
  struct { real32 x; real32 y; real32 z; };
} vec3_t;

typedef union vec4_t {
  real32 v[4];
  struct { real32 x; real32 y; real32 z; real32 w; };
} vec4_t;

typedef union quaternion_t {
  struct { real32 x; real32 y; real32 z; };
  struct { real32 i; real32 j; real32 k; };
  struct { vec3_t v; real32 w; };
} quaternion_t;

typedef union mat2_t {
  vec2_t v[2];
  real32 m[2][2];
} mat2_t;

typedef union mat3_t {
  vec3_t v[3];
  real32 m[3][3];
} mat3_t;

typedef union mat4_t {
  vec4_t v[4];
  real32 m[4][4];
} mat4_t;

typedef struct triangle_t {
  vec3_t p1;
  vec3_t p2;
  vec3_t p3;
} triangle_t;


// Game specific

typedef struct {
  void *memory;
  i32   width;
  i32   height;
  i32   bytes_per_pixel;
  i32   pitch;
  i32   bitmap_memory_size;
} game_offscreen_buffer_t;


typedef struct game_sound_output_t {
  i32  samples_per_sec;
  u32  samples_count;
  i16 *samples;
} game_sound_output_t;


typedef enum game_key_state_e {
  KEY_RELEASED,
  KEY_PRESSED,
} game_key_state_e;

typedef enum game_key_e {
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

typedef struct game_key_t {
  u8 press_rel;
} game_key_t;

typedef struct game_input_t {
  game_key_t keys[LOGIC_KEYS_N];
} game_input_t;


typedef struct game_state_t {

  bool32 running;
  real32 dt;

  game_input_t prev_input;

  // sound
  real32 volume;
  real32 freq;
  real32 f_step;
  real32 f_inv_step;
  real32 t_step;
  real32 t;
  bool32 start;
  bool32 start_samp_count;
  bool32 start_samp_total;

  // camera
  struct {
    real32 fov;

    vec3_t position;
    vec3_t velocity;

    real32 _far;
    real32 _near;

    // R U F
    quaternion_t rotation;
    vec3_t       rotation_velocity;
  } camera;

  triangle_t triangle[20];
  u32        color[20];

} game_state_t;


typedef struct game_memory_t {
  bool32  is_initialized;

  u64     permanent_storage_size;
  u8     *permanent_storage;

  u64     transient_storage_size;
  u8     *transient_storage;

} game_memory_t;


typedef u64 game_time_t;


