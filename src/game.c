#include "game.h"
#include "platform.h"

// temporary...
#include <windows.h>
#include <stdlib.h>
#include <time.h>

#include "math.c"

#define UP ((vec3_t) { 0 , 1.0F, 0 })
#define SPEED 60


INTERNAL void update_camera(game_state_t *game_state)
{

  vec3_t rotation_velocity = game_state->camera.rotation_velocity;

  real32 roll = DEG2RAD(rotation_velocity.x) * game_state->dt;
  while (roll < 0)    roll += 2*PI;
  while (roll > 2*PI) roll -= 2*PI;
  game_state->camera.rotation = 
    math_quaternion_mul(
      math_quaternion((vec3_t) { 1, 0, 0 }, roll),
      game_state->camera.rotation);

  real32 pitch = DEG2RAD(rotation_velocity.y) * game_state->dt;
  while (pitch < 0)    pitch += 2*PI;
  while (pitch > 2*PI) pitch -= 2*PI;
  game_state->camera.rotation = 
    math_quaternion_mul(
      math_quaternion((vec3_t) { 0, -1, 0 }, pitch),
      game_state->camera.rotation);

  real32 yaw = DEG2RAD(rotation_velocity.z) * game_state->dt;
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


INTERNAL void render_background(game_offscreen_buffer_t *offscreen_buffer,
                                game_state_t            *game_state)
{
  real32 w_factor = (real32)1024.0 / offscreen_buffer->width;
  real32 h_factor = (real32)1024.0 / offscreen_buffer->height;

  u8 *row = (u8 *)offscreen_buffer->memory;
  for (int y = 0; y < offscreen_buffer->height; ++y) {
    u32 *pixel = (u32 *)row;
    for (int x = 0; x < offscreen_buffer->width; ++x) {

      *pixel = 0xEEDDCCBB;

      ++pixel;
    }

    row += offscreen_buffer->pitch;
  }
}


INTERNAL void render_triangle(game_offscreen_buffer_t *offscreen_buffer,
                              game_state_t            *game_state,
                              triangle_t              *triangle,
                              u32                      color)
{
  i32 width  = offscreen_buffer->width;
  i32 height = offscreen_buffer->height;
  real32 tan_fov = 1/(real32)tanf(game_state->camera.fov/2);

  vec3_t triangle_norm = math_triangle_norm(triangle);
  triangle_norm = math_mul_c_v3(-1.0F/math_norm_v3(triangle_norm), triangle_norm);

  // dont render triangles that doesn't face the camera
  vec3_t cam_to_tri = math_sub_v3(game_state->camera.position, math_triangle_center(triangle));
  real32 dot = math_dot_prod_3(cam_to_tri, triangle_norm);
  if (dot > 0)
    return;


  // move to camera
  vec3_t moved_p[3] = {
    math_sub_v3(triangle->p1, game_state->camera.position),
    math_sub_v3(triangle->p2, game_state->camera.position),
    math_sub_v3(triangle->p3, game_state->camera.position),
  };

  // rotate by camera rotation
  vec3_t cam_p[3] = {
    math_quaternion_rotate(game_state->camera.rotation, moved_p[0]),
    math_quaternion_rotate(game_state->camera.rotation, moved_p[1]),
    math_quaternion_rotate(game_state->camera.rotation, moved_p[2]),
  };

  // screen projection
  vec2_t p[3];

     for (u8 i = 0; i < 3; ++i) {
    if (cam_p[i].z < 0)
      return;

    p[i].x = cam_p[i].x / (cam_p[i].z*tan_fov);
    p[i].x = (p[i].x + 1.0F) * width/2.0F;

    p[i].y = cam_p[i].y / (cam_p[i].z*tan_fov);
    p[i].y = (p[i].y + 1.0F) * height/2.0F;
  }


  // sort points from top to bottom (ascending in y)
  for (u8 i = 0; i < 3; ++i) {
    for (u8 j = i+1; j < 3; ++j) {

      if (p[i].y >= p[j].y) {
        vec2_t tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
        continue;
      }
    }
  }

  real32 m[3];
  bool32 horizontal[3];
  for (u8  k = 0; k < 3; ++k) {
    real32 dy = p[(k+1)%3].y - p[k].y;
    real32 dx = p[(k+1)%3].x - p[k].x;

    if (dy > -1.0F && dy < 1.0F) {
      horizontal[k] = TRUE;
    }
    else {
      horizontal[k] = FALSE;
      m[k] = dx / dy;
    }
  }


  // draw 2 triangles

  u8 *row;
  i32 y;
  real32 y_max;

  if (p[0].y > height) return;

  if (p[1].y > 0) {

    y = (u32)p[0].y;
    if (y < 0) y = 0;
    if (y > height-1) y = height-1;

    y_max = (real32)(p[1].y - y);
    if (y_max > height-y) y_max = (real32)(height-y);

    row = (u8 *)offscreen_buffer->memory + y*offscreen_buffer->pitch;
    for (u32 dy = 0; dy < y_max; ++dy) {
      i32 x_min, x_max;

      // x = x1 + m(y - y1)

      if (!horizontal[0]) x_min = (u32)(p[1].x + (y - p[1].y + dy)*m[0]);
      else x_min = (u32)p[1].x;

      if (!horizontal[2]) x_max = (u32)(p[2].x + (y - p[2].y + dy)*m[2]);
      else x_max = (u32)p[2].x;

      if (x_min > x_max) {
        i32 tmp = x_min;
        x_min = x_max;
        x_max = tmp;
      }

      if (x_min < 0) x_min = 0;
      if (x_min > width-1) x_min = width-1;
      if (x_max < 0) x_max = 0;
      if (x_max > width-1) x_max = width-1;

      u32 *pixel = (u32 *)row + x_min;
      for (i32 x = x_min; x < x_max; ++x) {

        *pixel = color;

        ++pixel;
      }

      row += offscreen_buffer->pitch;
    }
  }

  if (p[1].y > height) return;

  y = (u32)p[1].y;
  if (y < 0) y = 0;

  y_max = (real32)(p[2].y - y);
  if (y_max > height-y) y_max = (real32)(height-y);

  row = (u8 *)offscreen_buffer->memory + y*offscreen_buffer->pitch;
  for (u32 dy = 0; dy < y_max; ++dy) {
    i32 x_min, x_max;

    if (!horizontal[1]) x_min = (u32)(p[2].x + (y - p[2].y + dy)*m[1]);
    else x_min = (u32)p[1].x;

    if (!horizontal[2]) x_max = (u32)(p[2].x + (y - p[2].y + dy)*m[2]);
    else x_max = (u32)p[0].x;

    if (x_min > x_max) {
      i32 tmp = x_min;
      x_min = x_max;
      x_max = tmp;
    }

    if (x_min < 0) x_min = 0;
    if (x_min > width-1) x_min = width-1;
    if (x_max < 0) x_max = 0;
    if (x_max > width-1) x_max = width-1;

    u32 *pixel = (u32 *)row + x_min;
    for (i32 x = x_min; x < x_max; ++x) {

      *pixel = color;

      ++pixel;
    }

    row += offscreen_buffer->pitch;
  }
}


INTERNAL void render_triangles(game_offscreen_buffer_t *offscreen_buffer,
                               game_state_t            *game_state)
{
  LOCAL_PERSIST u32 timeout;

  // startup
  if ((timeout++) % (30*10) == 0) {

    LOCAL_PERSIST u32 s = 1;
    if (s == 0) {

      LOCAL_PERSIST u32 width  = 100;
      LOCAL_PERSIST u32 height = 100;
      u32 t = (u32)platform_get_clock_f();
      srand(t);

      for (u8 i = 0; i < 10; ++i) {
        real32 x1 = (real32)(rand() % width) - width / 2.0F;
        real32 x2 = (real32)(rand() % width) - width / 2.0F;
        real32 x3 = (real32)(rand() % width) - width / 2.0F;

        real32 y1 = (real32)(rand() % height) - height / 2.0F;
        real32 y2 = (real32)(rand() % height) - height / 2.0F;
        real32 y3 = (real32)(rand() % height) - height / 2.0F;

        real32 z1 = (real32)(rand() % width) - width / 2.0F;
        real32 z2 = (real32)(rand() % width) - width / 2.0F;
        real32 z3 = (real32)(rand() % width) - width / 2.0F;

        game_state->color[i] = (rand() % 256)      |
                               (rand() % 256) << 8 |
                               (rand() % 256) << 16;

        game_state->triangle[i] = (triangle_t) {
          .p1 = (vec3_t) {.x =  x1, .y = y1, .z = z1},
          .p2 = (vec3_t) {.x =  x2, .y = y2, .z = z2},
          .p3 = (vec3_t) {.x =  x3, .y = y3, .z = z3},
        };
      }
    }
    if (s == 1) {

      LOCAL_PERSIST u32 n = 10;
      LOCAL_PERSIST real32 r = 100;

      for (u8 i = 0; i < n; ++i) {

        real32 thetha = 2*PI/n;
        real32 thetha_1 = thetha*i;
        real32 thetha_2 = thetha*((i+1)%n);

        real32 x1 = 0;
        real32 x2 = r*sinf(thetha_1);
        real32 x3 = r*sinf(thetha_2);

        real32 z1 = 0;
        real32 z2 = r*cosf(thetha_1);
        real32 z3 = r*cosf(thetha_2);


        real32 y1 = 2.8F*r;
        real32 y2 = -r;
        real32 y3 = -r;

        game_state->color[i+10] =
        game_state->color[i] = (rand() % 256)      |
                               (rand() % 256) << 8 |
                               (rand() % 256) << 16;

        game_state->triangle[i+10] = (triangle_t) {
          .p1 = (vec3_t) {.x =  x1, .y = y1, .z = z1},
          .p2 = (vec3_t) {.x =  x2, .y = y2, .z = z2},
          .p3 = (vec3_t) {.x =  x3, .y = y3, .z = z3},
        };

        game_state->triangle[i] = (triangle_t) {
          .p1 = (vec3_t) {.x =  x1, .y = y1, .z = z1},
          .p2 = (vec3_t) {.x =  x3, .y = y2, .z = z3},
          .p3 = (vec3_t) {.x =  x2, .y = y3, .z = z2},
        };
      }
    }
  }

  render_triangle(offscreen_buffer, game_state,
      &(triangle_t) {
      .p1 = { -2000, -1000, -2000 },
      .p2 = {  2000, -1000,  2000 },
      .p3 = {  2000, -1000, -2000 },
      }, 0x008888FF);

  render_triangle(offscreen_buffer, game_state,
      &(triangle_t) {
      .p1 = { -2000, -1000, -2000 },
      .p2 = { -2000, -1000,  2000 },
      .p3 = {  2000, -1000,  2000 },
      }, 0x008888FF);

  for (u8 i = 0; i < 20; ++i) {
    render_triangle(offscreen_buffer, game_state,
                    &game_state->triangle[i], game_state->color[i]);
  }

}



INTERNAL void game_output_sound(game_sound_output_t *sound_buffer,
                                game_state_t        *game_state)
{
  i16 *samples = sound_buffer->samples;

  if (game_state->start) {
    for (u32 sample_idx = 0; sample_idx < sound_buffer->samples_count; ++sample_idx) {
      real32 val = (real32)(sinf(game_state->t));

      i16 sample_val = (i16)(val*game_state->volume*
                       ((real32)(game_state->start_samp_count+sample_idx)/game_state->start_samp_total));

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

      real32 val = (real32)(sinf(game_state->t));

      i16 sample_val = (i16)(val*game_state->volume);

      samples[0] = samples[1] = sample_val;
      samples += 2;

      game_state->t += game_state->t_step;
      if (game_state->t > 2*PI)
        game_state->t -= 2*PI;
    }
  }
}


INTERNAL void game_input(game_input_t      *input,
                         game_state_t      *game_state)
{
  for (i32 key = 0; key < LOGIC_KEYS_N; ++key) {
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
        game_state->camera.velocity.x = SPEED;
      } break;

      case LOGIC_KEY_LEFT: {
        game_state->camera.velocity.x = -SPEED;
      } break;

      case LOGIC_KEY_UP: {
        game_state->camera.velocity.y = SPEED;
      } break;

      case LOGIC_KEY_DOWN: {
        game_state->camera.velocity.y = -SPEED;
      } break;

      case LOGIC_KEY_FORWARD: {
        game_state->camera.velocity.z = SPEED;
      } break;

      case LOGIC_KEY_BACKWARD: {
        game_state->camera.velocity.z = -SPEED;
      } break;

      // Rotation
      case LOGIC_KEY_ROT_UP: {
        game_state->camera.rotation_velocity.x = SPEED;
      } break;

      case LOGIC_KEY_ROT_DOWN: {
        game_state->camera.rotation_velocity.x = -SPEED;
      } break;

      case LOGIC_KEY_ROT_RIGHT: {
        game_state->camera.rotation_velocity.y = SPEED;
      } break;

      case LOGIC_KEY_ROT_LEFT: {
        game_state->camera.rotation_velocity.y = -SPEED;
      } break;

      case LOGIC_KEY_ROT_FORWARD: {
        game_state->camera.rotation_velocity.z = SPEED;
      } break;

      case LOGIC_KEY_ROT_BACKWARD: {
        game_state->camera.rotation_velocity.z = -SPEED;
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


void game_get_sound_samples(game_memory_t       *game_memory,
                            game_sound_output_t *sound_output_buffer)
{
  ASSERT(game_memory->permanent_storage_size >= sizeof(game_state_t));

  game_state_t *game_state = (game_state_t *)game_memory->permanent_storage;
  game_state->t_step  = 2*PI*game_state->freq/sound_output_buffer->samples_per_sec;
  game_output_sound(sound_output_buffer, game_state);
}


void game_update_and_render(game_memory_t           *game_memory,
                            game_offscreen_buffer_t *offscreen_buffer,
                            game_input_t            *input,
                            real32                   dt)
{
  ASSERT(game_memory->permanent_storage_size >= sizeof(game_state_t));

  game_state_t *game_state = (game_state_t *)game_memory->permanent_storage;
  if (!game_memory->is_initialized) {

    game_state->running    = TRUE;
    game_state->volume     = 0;
    game_state->freq       = 440.0;
    game_state->f_step     = 1.05946309436F; // 2^(1/12)
    game_state->f_inv_step = 0.94387431268F; // 2^(-1/12)
    game_state->t          = 0;
    game_state->start      = TRUE;
    game_state->start_samp_count = 0;
    game_state->start_samp_total = 48000/3;


    game_state->camera.fov = DEG2RAD(60);

    game_state->camera.position = (vec3_t) { 0, 0, -1000 };
    game_state->camera.velocity = (vec3_t) { 0 };

    game_state->camera.rotation = (quaternion_t) { .w = 1.0F };
    game_state->camera.rotation_velocity = (vec3_t) { 0 };

    game_memory->is_initialized = TRUE;
  }

  game_state->dt = dt;

  game_input(input, game_state);

  render_background(offscreen_buffer, game_state);
 
  update_camera(game_state);

  render_triangles(offscreen_buffer, game_state);
}


