
#include "game.h"
#undef PLATFORM
#include "api.h"

#include "arena.c"

// temporary...
#include "math.c"
#include <stdlib.h>
#include <stdio.h>

#include "lz77.c"
#include "png.c"

#include "json.c"
#include "glb.c"


GLOBAL_VARIABLE game_state_t *game_state;

#include "renderer.c"


#define MOV_SPEED 10
#define ROT_SPEED 180


INTERNAL void update_camera(void)
{

  vec3_t rotation_velocity = game_state->camera.rotation_velocity;

  r32 roll = DEG2RAD(rotation_velocity.x) * game_state->dt;
  while (roll < 0)    roll += 2*PI;
  while (roll > 2*PI) roll -= 2*PI;
  game_state->camera.rotation = 
    math_quaternion_mul(
      math_quaternion((vec3_t) { 1, 0, 0 }, roll),
      game_state->camera.rotation);

  r32 pitch = DEG2RAD(rotation_velocity.y) * game_state->dt;
  while (pitch < 0)    pitch += 2*PI;
  while (pitch > 2*PI) pitch -= 2*PI;
  game_state->camera.rotation = 
    math_quaternion_mul(
      math_quaternion((vec3_t) { 0, -1, 0 }, pitch),
      game_state->camera.rotation);

  r32 yaw = DEG2RAD(rotation_velocity.z) * game_state->dt;
  while (yaw < 0)    yaw += 2*PI;
  while (yaw > 2*PI) yaw -= 2*PI;
  game_state->camera.rotation = 
    math_quaternion_mul(
      math_quaternion((vec3_t) { 0, 0, 1 }, yaw),
      game_state->camera.rotation);

  vec3_t camera_direction_x = math_mul_c_v3(game_state->camera.velocity.x, math_quaternion_rotate_inv(game_state->camera.rotation, (vec3_t) { 1, 0, 0 }));
  vec3_t camera_direction_y = math_mul_c_v3(game_state->camera.velocity.y, math_quaternion_rotate_inv(game_state->camera.rotation, (vec3_t) { 0, 1, 0 }));
  vec3_t camera_direction_z = math_mul_c_v3(game_state->camera.velocity.z, math_quaternion_rotate_inv(game_state->camera.rotation, (vec3_t) { 0, 0, 1 }));

  // advance pos by velocity
  game_state->camera.position = math_add_v3(game_state->camera.position, camera_direction_x);
  game_state->camera.position = math_add_v3(game_state->camera.position, camera_direction_y);
  game_state->camera.position = math_add_v3(game_state->camera.position, camera_direction_z);
}


INTERNAL void render_triangles(void)
{
  game_offscreen_buffer_t *offscreen_buffer = &game_state->offscreen_buffer;
  game_depth_buffer_t *depth_buffer = &game_state->depth_buffer;

  // startup
  LOCAL_PERSIST u32 s = 2;

#define TEST_N_TRIANGLES 10
  if (s == 0) {
    LOCAL_PERSIST u32 width  = 100;
    LOCAL_PERSIST u32 height = 100;
    
    u32 t = (u32)platform_get_clock_f();
    srand(t);

    for (u8 i = 0; i < TEST_N_TRIANGLES; ++i) {
      r32 x1 = (r32)(rand() % width) - width / 2.0F;
      r32 x2 = (r32)(rand() % width) - width / 2.0F;
      r32 x3 = (r32)(rand() % width) - width / 2.0F;

      r32 y1 = (r32)(rand() % height) - height / 2.0F;
      r32 y2 = (r32)(rand() % height) - height / 2.0F;
      r32 y3 = (r32)(rand() % height) - height / 2.0F;

      r32 z1 = (r32)(rand() % width) - width / 2.0F;
      r32 z2 = (r32)(rand() % width) - width / 2.0F;
      r32 z3 = (r32)(rand() % width) - width / 2.0F;

      u32 color = (rand() % 256)      |
                  (rand() % 256) << 8 |
                  (rand() % 256) << 16;

      triangle_t triangle = (triangle_t) {
        .p1 = (vec3_t) {.x =  x1, .y = y1, .z = z1},
        .p2 = (vec3_t) {.x =  x2, .y = y2, .z = z2},
        .p3 = (vec3_t) {.x =  x3, .y = y3, .z = z3},
      };

      render_triangle(offscreen_buffer, depth_buffer, &triangle, color);
    }
  }
  if (s == 1) {

    r32 r = 100;

    for (u8 i = 0; i < TEST_N_TRIANGLES; ++i) {

      r32 thetha = 2*PI/TEST_N_TRIANGLES;
      r32 thetha_1 = thetha*i;
      r32 thetha_2 = thetha*((i+1)%TEST_N_TRIANGLES);

      r32 x1 = 0;
      r32 x2 = r*sinf(thetha_1);
      r32 x3 = r*sinf(thetha_2);

      r32 z1 = 0;
      r32 z2 = r*cosf(thetha_1);
      r32 z3 = r*cosf(thetha_2);


      r32 y1 = 2.8F*r;
      r32 y2 = -r;
      r32 y3 = -r;

      u32 color = (rand() % 256)      |
                  (rand() % 256) << 8 |
                  (rand() % 256) << 16;

      triangle_t triangle = (triangle_t) {
        .p1 = (vec3_t) {.x =  x1, .y = y1, .z = z1},
        .p2 = (vec3_t) {.x =  x2, .y = y2, .z = z2},
        .p3 = (vec3_t) {.x =  x3, .y = y3, .z = z3},
      };
      render_triangle(offscreen_buffer, depth_buffer, &triangle, color);

      triangle = (triangle_t) {
        .p1 = (vec3_t) {.x =  x1, .y = y1, .z = z1},
        .p2 = (vec3_t) {.x =  x3, .y = y2, .z = z3},
        .p3 = (vec3_t) {.x =  x2, .y = y3, .z = z2},
      };
      render_triangle(offscreen_buffer, depth_buffer, &triangle, color);
    }
  }

  game_time_t time_start = platform_get_clock_f();

  for (u32 i = 0; i < game_state->sim_region.triangle_count; ++i)
    render_triangle(offscreen_buffer, depth_buffer, &game_state->sim_region.triangles[i], 0xBBCCDDEE);

  game_time_t time_stop = platform_get_clock_f();
  u64 time_diff = (u64)USEC2MSEC(time_stop - time_start);
  printf("%lld\n", time_diff);
}


INTERNAL void game_output_sound(game_sound_output_t *sound_buffer)
{
  s16 *samples = sound_buffer->samples;

  if (game_state->start) {
    for (u32 sample_idx = 0; sample_idx < sound_buffer->samples_count; ++sample_idx) {
      r32 val = (r32)(sinf(game_state->t));

      s16 sample_val = (s16)(val*game_state->volume*
                       ((r32)(game_state->start_samp_count+sample_idx)/game_state->start_samp_total));

      samples[0] = samples[1] = sample_val;
      samples += 2;

      game_state->t += game_state->t_step;
      if (game_state->t > 2*PI)
        game_state->t -= 2*PI;
    }

    game_state->start_samp_count += sound_buffer->samples_count;
    if (game_state->start_samp_count > game_state->start_samp_total)
      game_state->start = FALSE;
  }
  else {
    for (u32 sample_idx = 0; sample_idx < sound_buffer->samples_count; ++sample_idx) {

      r32 val = (r32)(sinf(game_state->t));

      s16 sample_val = (s16)(val*game_state->volume);

      samples[0] = samples[1] = sample_val;
      samples += 2;

      game_state->t += game_state->t_step;
      if (game_state->t > 2*PI)
        game_state->t -= 2*PI;
    }
  }
}


void game_init(game_memory_t *game_memory, b32 hard)
{
  u32 global_size = (u32)(0.7*game_memory->memory_size);
  u32 frame_size  = (u32)(0.3*game_memory->memory_size);

  game_state = (game_state_t *)game_memory->memory;
  if (!hard)
    return;

  game_state->global_arena = arena_init(game_memory->memory, global_size);
  arena_alloc(&game_state->global_arena, sizeof(game_state_t));
  
  game_state->frame_arena = arena_init(game_memory->memory+global_size, frame_size);

  game_state->running    = TRUE;
  game_state->volume     = 0;
  game_state->freq       = 440.0;
  game_state->f_step     = 1.05946309436F; // 2^(1/12)
  game_state->f_inv_step = 0.94387431268F; // 2^(-1/12)
  game_state->t          = 0;
  game_state->start      = TRUE;
  game_state->start_samp_count = 0;
  game_state->start_samp_total = 48000/3;

  game_state->camera.fov = DEG2RAD(120);

  game_state->camera.position = (vec3_t) { 0, 0, -1000 };
  game_state->camera.velocity = (vec3_t) { 0 };
  
  game_state->camera.far  = 10000;
  game_state->camera.near = 1;

  game_state->camera.rotation = (quaternion_t) { .w = 1.0F };
  game_state->camera.rotation_velocity = (vec3_t) { 0 };

#define MAX_TRIANGLES 1000000
  game_state->sim_region.triangles = arena_alloc(&game_state->global_arena,
                                                 MAX_TRIANGLES*sizeof(triangle_t));
  game_state->sim_region.triangle_count = 0;

#if 1
  if (glb_test(&game_state->sim_region))
    return;
#endif
}


void game_get_sound_samples(game_sound_output_t *sound_output_buffer)
{
  game_state->t_step  = 2*PI*game_state->freq/sound_output_buffer->samples_per_sec;
  game_output_sound(sound_output_buffer);
}


INTERNAL void game_input(game_input_t *input)
{
  for (s32 key = 0; key < LOGIC_KEYS_N; ++key) {
    if (input->keys[key].press_rel == game_state->prev_input.keys[key].press_rel)
      continue;

    if (input->keys[key].press_rel == KEY_RELEASED) {
      switch (key) {
        case LOGIC_KEY_RIGHT:
        case LOGIC_KEY_LEFT: {
          game_state->camera.velocity.x = 0;
        } break;

        case LOGIC_KEY_UP:
        case LOGIC_KEY_DOWN: {
          game_state->camera.velocity.y = 0;
        } break;

        case LOGIC_KEY_FORWARD:
        case LOGIC_KEY_BACKWARD: {
          game_state->camera.velocity.z = 0;
        } break;

        case LOGIC_KEY_ROT_UP:
        case LOGIC_KEY_ROT_DOWN: {
          game_state->camera.rotation_velocity.x = 0;
        } break;

        case LOGIC_KEY_ROT_RIGHT:
        case LOGIC_KEY_ROT_LEFT: {
          game_state->camera.rotation_velocity.y = 0;
        } break;

        case LOGIC_KEY_ROT_FORWARD:
        case LOGIC_KEY_ROT_BACKWARD: {
          game_state->camera.rotation_velocity.z = 0;
        } break;

        default: break;
      }
      continue;
    }

    switch (key) {

      // Movement
      case LOGIC_KEY_RIGHT: {
        game_state->camera.velocity.x = MOV_SPEED;
      } break;

      case LOGIC_KEY_LEFT: {
        game_state->camera.velocity.x = -MOV_SPEED;
      } break;

      case LOGIC_KEY_UP: {
        game_state->camera.velocity.y = MOV_SPEED;
      } break;

      case LOGIC_KEY_DOWN: {
        game_state->camera.velocity.y = -MOV_SPEED;
      } break;

      case LOGIC_KEY_FORWARD: {
        game_state->camera.velocity.z = MOV_SPEED;
      } break;

      case LOGIC_KEY_BACKWARD: {
        game_state->camera.velocity.z = -MOV_SPEED;
      } break;

      // Rotation
      case LOGIC_KEY_ROT_UP: {
        game_state->camera.rotation_velocity.x = ROT_SPEED;
      } break;

      case LOGIC_KEY_ROT_DOWN: {
        game_state->camera.rotation_velocity.x = -ROT_SPEED;
      } break;

      case LOGIC_KEY_ROT_RIGHT: {
        game_state->camera.rotation_velocity.y = ROT_SPEED;
      } break;

      case LOGIC_KEY_ROT_LEFT: {
        game_state->camera.rotation_velocity.y = -ROT_SPEED;
      } break;

      case LOGIC_KEY_ROT_FORWARD: {
        game_state->camera.rotation_velocity.z = ROT_SPEED;
      } break;

      case LOGIC_KEY_ROT_BACKWARD: {
        game_state->camera.rotation_velocity.z = -ROT_SPEED;
      } break;

      case LOGIC_KEY_ATTACK: {
        game_state->freq *= game_state->f_step;
      } break;

      case LOGIC_KEY_DEFEND: {
        game_state->freq *= game_state->f_inv_step;
      } break;

      case LOGIC_KEY_HEAL: {
        game_state->freq = 440.0;
      } break;

      case LOGIC_KEY_QUIT: {
        game_state->running = FALSE;
      } break;

      default: break;
    }
  }

  game_state->prev_input = *input;
}


int cuda_test(void);

void game_update_and_render(game_offscreen_buffer_t *offscreen_buffer,
                            game_input_t            *input,
                            r32                      dt)
{
  game_state->frame_arena.top = 0;
  game_state->offscreen_buffer = *offscreen_buffer;
  game_state->depth_buffer.width  = offscreen_buffer->width;
  game_state->depth_buffer.height = offscreen_buffer->height;
  game_state->depth_buffer.pitch  = offscreen_buffer->width*sizeof(r32);
  u32 depth_buffer_size = offscreen_buffer->width * offscreen_buffer->height;
  game_state->depth_buffer.memory = arena_alloc(&game_state->frame_arena, depth_buffer_size*sizeof(r32));
  for (u32 i = 0; i < depth_buffer_size; ++i)
    game_state->depth_buffer.memory[i] = 0.0;

#if 0
  if (png_test(offscreen_buffer))
    return;
#endif

  game_state->dt = dt;

  game_input(input);

  render_background(offscreen_buffer);
 
  update_camera();

  render_triangles();

  cuda_test();
}



#include <cuda.h>


int cuda_test(void)
{
  static u32 _ = 0;
  if (_++) return 0;

#define CHECK_CUDA(call)                                                    \
    do {                                                                    \
        CUresult result = call;                                             \
        if (result != CUDA_SUCCESS) {                                       \
            const char* errStr;                                             \
            cuGetErrorString(result, &errStr);                              \
            printf("CUDA Error at line %d: %s\n", __LINE__, errStr);        \
            exit(1);                                                        \
        }                                                                   \
    } while(0)

#define N 1024
  printf("CUDA start\n");
  r32 cpu_a[N];
  r32 cpu_b[N];
  r32 cpu_c[N];

  for (u32 i = 0; i < N; ++i) {
    cpu_a[i] = (r32)i;
    cpu_b[i] = (r32)i*2;
  }

  CHECK_CUDA(cuInit(0));
  CUdevice device;
  CHECK_CUDA(cuDeviceGet(&device, 0));
  CUcontext context;
  CHECK_CUDA(cuCtxCreate(&context, NULL, 0, device));

  CUdeviceptr gpu_a;
  CUdeviceptr gpu_b;
  CUdeviceptr gpu_c;
  u64 bytes = N*sizeof(r32);
  CHECK_CUDA(cuMemAlloc(&gpu_a, bytes));
  CHECK_CUDA(cuMemAlloc(&gpu_b, bytes));
  CHECK_CUDA(cuMemAlloc(&gpu_c, bytes));

  CHECK_CUDA(cuMemcpyHtoD(gpu_a, cpu_a, bytes));
  CHECK_CUDA(cuMemcpyHtoD(gpu_b, cpu_b, bytes));

  
  u32 ptx_file_size = 0;
  u8 *ptx_source = platform_DEBUG_read_entire_file_f("W:\\game\\src\\test.ptx", &ptx_file_size);

  // Prepare diagnostic buffers to intercept the JIT error report
  char error_log[8192] = {0};
  CUjit_option options[] = { CU_JIT_ERROR_LOG_BUFFER, CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES };
  void* option_values[] = { error_log, (void*)(uintptr_t)sizeof(error_log) }; // Fixed value casting

  CUmodule module;
  CUresult res = cuModuleLoadDataEx(&module, ptx_source, 2, options, option_values);
  if (res != CUDA_SUCCESS) {
    printf("=== PTX JIT Compiler Error Log ===\n%s\n==================================\n", error_log);
  }

  CHECK_CUDA(cuModuleLoad(&module, "W:\\game\\src\\test.ptx"));
  CUfunction function;
  CHECK_CUDA(cuModuleGetFunction(&function, module, "add_arrays"));

  s32 threads_per_block = 256;
  s32 blocks_per_grid = (N + threads_per_block - 1) / threads_per_block;
  s64 size = N;
  void *args[] = { &gpu_a, &gpu_b, &gpu_c, &size };

  CHECK_CUDA(cuLaunchKernel(function, 
                            blocks_per_grid, 1, 1,    // Grid dimensions
                            threads_per_block, 1, 1,  // Block dimensions
                            0, NULL,                  // Shared memory and stream
                            args, NULL));             // Kernel parameters

  // CUresult CUDAAPI cuLaunchKernel(CUfunction f,
  //                                 unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
  //                                 unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
  //                                 unsigned int sharedMemBytes, CUstream hStream,
  //                                 void **kernelParams, void **extra);

  CHECK_CUDA(cuMemcpyDtoH(cpu_c, gpu_c, bytes));

  for (u32 i = 0; i < N; ++i)
    printf("%f + %f ? %f (%f)\n", cpu_a[i], cpu_b[i], cpu_c[i], (cpu_a[i] + cpu_b[i]));

  cuMemFree(gpu_a);
  cuMemFree(gpu_b);
  cuMemFree(gpu_c);
  cuCtxDestroy(context);

  return 0;
}


