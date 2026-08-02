
#include "game.h"
#include "platform.h"


INTERNAL void render_gradient(game_offscreen_buffer_t *offscreen_buffer,
                              game_state_t            *game_state)
{
  real32 w_factor = (real32)1024.0 / offscreen_buffer->width;
  real32 h_factor = (real32)1024.0 / offscreen_buffer->height;

  u8 *row = (u8 *)offscreen_buffer->memory;
  for (int y = 0; y < offscreen_buffer->height; ++y) {
    u32 *pixel = (u32 *)row;
    for (int x = 0; x < offscreen_buffer->width; ++x) {

      u8 blue  = (u8)(x*w_factor + game_state->blue_offset);
      u8 green = (u8)(y*h_factor + game_state->green_offset);

      *pixel = (green << 8) | (blue << 0);

      ++pixel;
    }

    row += offscreen_buffer->pitch;
  }
}


INTERNAL void game_output_sound(game_sound_output_t *sound_buffer,
                                game_state_t               *game_state)
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

    if (input->keys[key].press_rel == KEY_RELEASED)
      continue;

    switch (key) {
      case LOGIC_KEY_UP: {
        game_state->green_vel += 10;
      } break;

      case LOGIC_KEY_DOWN: {
        game_state->green_vel -= 10;
      } break;

      case LOGIC_KEY_RIGHT: {
        game_state->blue_vel += 10;
      } break;

      case LOGIC_KEY_LEFT: {
        game_state->blue_vel -= 10;
      } break;

      case LOGIC_KEY_STOP: {
        game_state->blue_vel  = 0;
        game_state->green_vel = 0;
      } break;


      case LOGIC_KEY_ATTACK: {
        game_state->freq   *= game_state->f_step;
      } break;

      case LOGIC_KEY_DEFEND: {
        game_state->freq   *= game_state->f_inv_step;
      } break;

      case LOGIC_KEY_HEAL: {
        game_state->freq = 440.0;
      } break;

      case LOGIC_KEY_QUIT: {
        game_state->running = FALSE;
      } break;
    }
  }

  game_state->prev_input = *input;
}


void game_get_sound_samples(game_memory_t       *game_memory,
                            game_sound_output_t *sound_output_buffer)
{
  ASSERT(sizeof(game_state_t) <= game_memory->permanent_storage_size);

  game_state_t *game_state = (game_state_t *)game_memory->permanent_storage;
  game_state->t_step  = 2*PI*game_state->freq/sound_output_buffer->samples_per_sec;
  game_output_sound(sound_output_buffer, game_state);
}


void game_update_and_render(game_memory_t           *game_memory,
                            game_offscreen_buffer_t *offscreen_buffer,
                            game_input_t            *input)
{
  ASSERT(sizeof(game_state_t) <= game_memory->permanent_storage_size);

  game_state_t *game_state = (game_state_t *)game_memory->permanent_storage;
  if (!game_memory->is_initialized) {

    game_state->running    = TRUE;
    game_state->volume     = 3000.0;
    game_state->freq       = 440.0;
    game_state->f_step     = 1.05946309436F; // 2^(1/12)
    game_state->f_inv_step = 0.94387431268F; // 2^(-1/12)
    game_state->t = 0;
    game_state->start = TRUE;
    game_state->start_samp_count = 0;
    game_state->start_samp_total = 48000/3;

    game_memory->is_initialized = TRUE;
  }

  game_state->blue_offset  += game_state->blue_vel;
  game_state->green_offset += game_state->green_vel;

  game_input(input, game_state);

  render_gradient(offscreen_buffer, game_state);
}


