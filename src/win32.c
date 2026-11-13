
#include "game.h"

#define PLATFORM
#include "api.h"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

// temporary...
#include <stdio.h>
#include <math.h>

#define GAME_UPDATE_HZ      60
#define SECONDS_PER_FRAME  (1.0F/GAME_UPDATE_HZ)
#define USECONDS_PER_FRAME ((u32)SEC2USEC(SECONDS_PER_FRAME))

#define SOUND_UPDATE_HZ 30
#define SOUND_LATENCY   (1.0F/SOUND_UPDATE_HZ)

typedef struct {
  int width;
  int height;
} win32_window_dimension_t;

typedef struct {
  BITMAPINFO  info;
  void *      memory;
  u32         width;
  u32         height;
  u32         bytes_per_pixel;
  u32         pitch;
  u32         bitmap_memory_size;
} win32_offscreen_buffer_t;


typedef struct {
  LPDIRECTSOUNDBUFFER  buffer;
  u32                  buffer_size;
  u32                  samples_per_sec;
  u32                  bytes_per_sample;
  u32                  cur_sample;
  u32                  safety_samples_bytes;
  s16                 *tmp_sound_buffer;
} win32_sound_output_t;

typedef struct win32_debug_sound_marker_t {
  DWORD flip_play_cursor;
  DWORD flip_write_cursor;

  DWORD output_play_cursor;
  DWORD output_write_cursor;

  DWORD output_location;
  DWORD output_byte_count;

  DWORD output_next_play_cursor;
} win32_debug_sound_marker_t;

typedef struct win32_input_t {
  u32 vk_code[LOGIC_KEYS_N];
  u8  press_rel[LOGIC_KEYS_N];
} win32_input_t;



GLOBAL_VARIABLE win32_offscreen_buffer_t win32_offscreen_buffer;
GLOBAL_VARIABLE win32_sound_output_t win32_sound_output;
GLOBAL_VARIABLE u64 win32_performence_frequency;
GLOBAL_VARIABLE struct  {
  b32 running:1;
  b32 resize:1;
} win32_state;


// win32 libraries
DEFINE_STUB(DWORD WINAPI, XInputGetState, \
            (DWORD dwUserIndex, XINPUT_STATE *pState), \
            ERROR_DEVICE_NOT_CONNECTED)

DEFINE_STUB(DWORD WINAPI, XInputSetState, \
            (DWORD dwUserIndex, XINPUT_VIBRATION *pVibration), \
            ERROR_DEVICE_NOT_CONNECTED)

DEFINE_STUB(HRESULT WINAPI, DirectSoundCreate, \
            (LPCGUID pcGuidDevicem, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter), 0)


// ------ //
//  INIT  //
// ------ //

INTERNAL inline FILETIME win32_get_last_write_time(char *file_name)
{
  FILETIME last_write_time = { 0 };

  WIN32_FIND_DATA find_data;
  HANDLE find_handle = FindFirstFileA(file_name, &find_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    last_write_time = find_data.ftLastWriteTime;
    FindClose(find_handle);
  }

  return last_write_time;
}


INTERNAL void win32_string_cat(char *src1, u32 src1_len,
                               char *src2, u32 src2_len,
                               char *dst,  u32 dst_len)
{
  u32 l1 = 0, l2 = 0;
  char *w = src1;

  if (dst_len < src1_len) src1_len = dst_len;
  for (u32 i = 0; i < src1_len; ++i)
    *(dst++) = *(w++);

  dst_len -= (u32)(w - src1);
  if (dst_len < src2_len) src2_len = dst_len;
  for (u32 i = 0; i < src2_len; ++i)
    *(dst++) = *(src2++);

  *dst = 0;
}


INTERNAL void win32_init_input(win32_input_t *win32_input)
{
  HMODULE xinput_lib = LoadLibraryA("xinput1_4.dll");
  if (!xinput_lib)
    xinput_lib = LoadLibraryA("xinput1_3.dll");
  if (xinput_lib) {
    XInputGetState_f = (XInputGetState_t *)GetProcAddress(xinput_lib, "XInputGetState");
    XInputSetState_f = (XInputSetState_t *)GetProcAddress(xinput_lib, "XInputSetState");
  }
  else {
      // TODO
  }

  win32_input->vk_code[LOGIC_KEY_RIGHT]     = 'D';
  win32_input->vk_code[LOGIC_KEY_LEFT]      = 'A';
  win32_input->vk_code[LOGIC_KEY_UP]        = 'K';
  win32_input->vk_code[LOGIC_KEY_DOWN]      = 'J';
  win32_input->vk_code[LOGIC_KEY_FORWARD]   = 'W';
  win32_input->vk_code[LOGIC_KEY_BACKWARD]  = 'S';

  win32_input->vk_code[LOGIC_KEY_ROT_UP]       = 'Y';
  win32_input->vk_code[LOGIC_KEY_ROT_DOWN]     = 'H';
  win32_input->vk_code[LOGIC_KEY_ROT_RIGHT]    = 'E';
  win32_input->vk_code[LOGIC_KEY_ROT_LEFT]     = 'Q';
  win32_input->vk_code[LOGIC_KEY_ROT_FORWARD]  = 'T';
  win32_input->vk_code[LOGIC_KEY_ROT_BACKWARD] = 'R';

  win32_input->vk_code[LOGIC_KEY_ATTACK] = 'X';
  win32_input->vk_code[LOGIC_KEY_DEFEND] = 'Z';
  win32_input->vk_code[LOGIC_KEY_HEAL]   = 'C';
  win32_input->vk_code[LOGIC_KEY_QUIT]   = ' ';
}


INTERNAL void win32_init_dsound(HWND window,
                                u32  samples_per_sec,
                                u32  nchannels)
{
  win32_sound_output.samples_per_sec  = samples_per_sec;
  win32_sound_output.bytes_per_sample = sizeof(s16)*nchannels;
  win32_sound_output.buffer_size      = win32_sound_output.samples_per_sec * win32_sound_output.bytes_per_sample;
  win32_sound_output.cur_sample       = 0;

  u32 bytes_per_sample = win32_sound_output.bytes_per_sample;
  win32_sound_output.safety_samples_bytes = (u32)(10.0/1000)*samples_per_sec*bytes_per_sample;

  HMODULE dsound_lib = LoadLibraryA("dsound.dll");

  if (dsound_lib) {
    DirectSoundCreate_f = (DirectSoundCreate_t *)GetProcAddress(dsound_lib, "DirectSoundCreate");

    LPDIRECTSOUND direct_sound;
    if (DirectSoundCreate_f && SUCCEEDED(DirectSoundCreate_f(NULL, &direct_sound, NULL))) {

      WAVEFORMATEX wave_format    = { 0 };
      wave_format.wFormatTag      = WAVE_FORMAT_PCM;
      wave_format.nChannels       = 2;
      wave_format.nSamplesPerSec  = samples_per_sec;
      wave_format.wBitsPerSample  = 16;
      wave_format.nBlockAlign     = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
      wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
      wave_format.cbSize          = 0;

      if (SUCCEEDED(direct_sound->lpVtbl->SetCooperativeLevel(direct_sound, window, DSSCL_PRIORITY))) {

        DSBUFFERDESC buffer_desc = { 0 };
        buffer_desc.dwSize  = sizeof(buffer_desc);
        buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

        LPDIRECTSOUNDBUFFER primary_buffer;
        if (SUCCEEDED(direct_sound->lpVtbl->CreateSoundBuffer(direct_sound,
                                                              &buffer_desc,
                                                              &primary_buffer,
                                                              NULL))) {

          if (SUCCEEDED(primary_buffer->lpVtbl->SetFormat(primary_buffer, &wave_format))) {
          }
          else {
            win32_state.running = FALSE;
            // TODO
          }
        }
        else {
          win32_state.running = FALSE;
          // TODO
        }
      }
      else {
        win32_state.running = FALSE;
        // TODO
      }

      DSBUFFERDESC buffer_desc = { 0 };
      buffer_desc.dwSize  = sizeof(buffer_desc);
      buffer_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
      buffer_desc.dwBufferBytes = win32_sound_output.buffer_size;
      buffer_desc.lpwfxFormat = &wave_format;
      if (SUCCEEDED(direct_sound->lpVtbl->CreateSoundBuffer(direct_sound,
                                                            &buffer_desc,
                                                            &win32_sound_output.buffer,
                                                            NULL))) {
      }
      else {
        win32_state.running = FALSE;
        // TODO
      }
    }
    else {
      win32_state.running = FALSE;
      // TODO
    }
  }
  else {
    win32_state.running = FALSE;
    // TODO
  }
}


INTERNAL void win32_clear_sound_buffer(void)
{
  VOID *region[2];
  DWORD region_size[2];
  if (SUCCEEDED(win32_sound_output.buffer->lpVtbl->Lock(win32_sound_output.buffer, 0,
                                                        win32_sound_output.buffer_size,
                                                        &region[0], &region_size[0],
                                                        &region[1], &region_size[1],
                                                        0))) {
    for (u32 reg_idx = 0; reg_idx < 2; ++reg_idx) {
      u8 *sample_out = (u8 *)region[reg_idx];
      for (u32 byte_idx = 0; byte_idx < region_size[reg_idx]; ++byte_idx) {
        *(sample_out++) = 0;
      }
    }

    win32_sound_output.buffer->lpVtbl->Unlock(win32_sound_output.buffer,
                                              region[0], region_size[0],
                                              region[1], region_size[1]);
  }
}


// ------- //
//  DEBUG  //
// ------- //

#ifdef GAME_INTERNAL
void platform_DEBUG_free_file_memory(void *memory)
{
  VirtualFree(memory, 0, MEM_RELEASE);
}


void *platform_DEBUG_read_entire_file(char *file_name, u32 *file_size)
{
  void *result = NULL;

  HANDLE file_handle = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (file_handle != INVALID_HANDLE_VALUE) {

    LARGE_INTEGER file_size_64;
    if (GetFileSizeEx(file_handle, &file_size_64)) {
      ASSERT(file_size_64.QuadPart > 0x00000000);
      DWORD file_size_32 = (u32)file_size_64.QuadPart;

      result = VirtualAlloc(NULL, file_size_32,
                            MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      if (result) {

        DWORD bytes_read;
        if (ReadFile(file_handle, result, file_size_32, &bytes_read, NULL) &&
            bytes_read == file_size_32) {
          *file_size = file_size_32;
        }
        else {
          platform_DEBUG_free_file_memory(result);
          result = NULL;
        }
      }
      else {
        // TODO
      }
    }
    else {
      // TODO
    }

    CloseHandle(file_handle);
  }
  else {
    // TODO
  }

  return result;
}


b32 platform_write_file(char *file_name,
                        u32   memory_size,
                        void *memory)
{
  b32 result = FALSE;

  HANDLE file_handle = CreateFile(file_name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (file_handle != INVALID_HANDLE_VALUE) {

    DWORD bytes_written;
    if (WriteFile(file_handle, memory, memory_size, &bytes_written, NULL)) {
      result = (bytes_written == memory_size);
    }
    else {
      // TODO
    }

    CloseHandle(file_handle);
  }
  else {
    // TODO
  }

  return result;
}


INTERNAL void win32_DEBUG_draw_vertical(win32_offscreen_buffer_t *offscreen_buffer,
                                        u32 x, u32 top, u32 bottom, u32 color)
{
  u8 *pixel = (u8 *)offscreen_buffer->memory +
    x*offscreen_buffer->bytes_per_pixel +
    top*offscreen_buffer->pitch;

  for (u32 y = top; y < bottom; ++y) {
    *(u32 *)pixel = color;
    pixel += offscreen_buffer->pitch;
  }
}


INTERNAL void win32_DEBUG_draw_rect(win32_offscreen_buffer_t *offscreen_buffer,
                                    u32 left, u32 right, u32 top, u32 bottom, u32 color)
{
  ASSERT(left >= 0 && right >= 0);
  ASSERT(left < offscreen_buffer->width && right < offscreen_buffer->width);
  ASSERT(left < right);
  ASSERT(bottom >= 0 && top >= 0);
  ASSERT(bottom < offscreen_buffer->height && top < offscreen_buffer->height);
  ASSERT(top < bottom);

  u8 *row = (u8 *)offscreen_buffer->memory +
            left*offscreen_buffer->bytes_per_pixel +
            top*offscreen_buffer->pitch;
  for (u32 y = top; y < bottom; ++y) {
    u32 *pixel = (u32 *)row;

    for (u32 x = left; x < right; ++x) {
      *pixel = color;
      ++pixel;
    }

    row += offscreen_buffer->pitch;
  }
}


INTERNAL void win32_DEBUG_sound_sync(win32_offscreen_buffer_t   *offscreen_buffer,
                                     win32_debug_sound_marker_t *sound_markers,
                                     u32                         sound_markers_count,
                                     u32                         current_marker_idx,
                                     win32_sound_output_t       *sound_buffer,
                                     r32                         seconds_per_frame)
{
  u32 pad_x  = 16;
  u32 pad_p  = 2;
  u32 pad_w  = 4;
  u32 pad_y  = 16;
  u32 line_height = 64;

  r32 step = ((r32)offscreen_buffer->width - 2*pad_x) / (r32)sound_buffer->buffer_size;

  for (u32 marker_idx = 0; marker_idx < sound_markers_count; ++marker_idx) {
    ASSERT(sound_markers[marker_idx].flip_play_cursor  < sound_buffer->buffer_size);
    ASSERT(sound_markers[marker_idx].flip_write_cursor < sound_buffer->buffer_size);

    u32 top    = pad_y;
    u32 bottom = pad_y + line_height;
    u32 x      = 0;

    if (marker_idx == current_marker_idx) {
      top    += line_height;
      bottom += line_height;
    }

    x = pad_x + (u32)(step * (r32)sound_markers[marker_idx].flip_play_cursor);
    win32_DEBUG_draw_rect(offscreen_buffer, x-pad_p, x+pad_p, top+pad_y, bottom-pad_y, 0x00008800);
    win32_DEBUG_draw_vertical(offscreen_buffer, x, top+pad_y, bottom-pad_y, 0x0000FF00);
    x = pad_x + (u32)(step * (r32)sound_markers[marker_idx].flip_write_cursor);
    win32_DEBUG_draw_rect(offscreen_buffer, x-pad_w, x+pad_w, top, bottom, 0x00880000);
    win32_DEBUG_draw_vertical(offscreen_buffer, x, top, bottom, 0x00FF0000);

    if (marker_idx == current_marker_idx) {
      top    += line_height;
      bottom += line_height;

      x = pad_x + (u32)(step * (r32)sound_markers[marker_idx].output_play_cursor);
      win32_DEBUG_draw_rect(offscreen_buffer, x-pad_p, x+pad_p, top+pad_y, bottom-pad_y, 0x00008800);
      win32_DEBUG_draw_vertical(offscreen_buffer, x, top+pad_y, bottom-pad_y, 0x0000FF00);
      x = pad_x + (u32)(step * (r32)sound_markers[marker_idx].output_write_cursor);
      win32_DEBUG_draw_rect(offscreen_buffer, x-pad_w, x+pad_w, top, bottom, 0x00880000);
      win32_DEBUG_draw_vertical(offscreen_buffer, x, top, bottom, 0x00FF0000);

      top    += line_height;
      bottom += line_height;

      x = pad_x + (u32)(step * (r32)sound_markers[marker_idx].output_location);
      win32_DEBUG_draw_rect(offscreen_buffer, x-pad_p, x+pad_p, top+pad_y, bottom-pad_y, 0x00008800);
      win32_DEBUG_draw_vertical(offscreen_buffer, x, top+pad_y, bottom-pad_y, 0x0000FF00);
      x = pad_x + (u32)(step * (r32)(sound_markers[marker_idx].output_location + sound_markers[marker_idx].output_byte_count));
      win32_DEBUG_draw_rect(offscreen_buffer, x-pad_w, x+pad_w, top, bottom, 0x00880000);
      win32_DEBUG_draw_vertical(offscreen_buffer, x, top, bottom, 0x00FF0000);

      top    += line_height;
      bottom += line_height;

      x = pad_x + (u32)(step * (r32)sound_markers[marker_idx].output_next_play_cursor);
      win32_DEBUG_draw_rect(offscreen_buffer, x-pad_w, x+pad_w, top, bottom, 0x00880000);
      win32_DEBUG_draw_vertical(offscreen_buffer, x, top, bottom, 0x00FF0000);
    }
  }
}
#endif


INTERNAL void win32_set_query_performence_frequency(void)
{
  LARGE_INTEGER query_performence_frequency;
  QueryPerformanceFrequency(&query_performence_frequency);

  // always in micro
  win32_performence_frequency = query_performence_frequency.QuadPart / 1000000;
}


// -------- //
//  WINDOW  //
// -------- //

INTERNAL win32_window_dimension_t win32_get_window_dimension(HWND window)
{
  RECT client_rect;
  GetClientRect(window, &client_rect);
  return (win32_window_dimension_t) {
    .width  = client_rect.right  - client_rect.left,
    .height = client_rect.bottom - client_rect.top
  };
}


INTERNAL void win32_resize_DIB_section(win32_offscreen_buffer_t *offset_buffer,
                                       int                       width,
                                       int                       height)
{
  offset_buffer->width  = width;
  offset_buffer->height = height;

  offset_buffer->info.bmiHeader.biSize = sizeof(offset_buffer->info.bmiHeader);
  offset_buffer->info.bmiHeader.biWidth = width;
  offset_buffer->info.bmiHeader.biHeight = height;
  offset_buffer->info.bmiHeader.biPlanes = 1;
  offset_buffer->info.bmiHeader.biBitCount = 32;
  offset_buffer->info.bmiHeader.biCompression = BI_RGB;

  offset_buffer->bytes_per_pixel = 4;
  offset_buffer->pitch = offset_buffer->width*offset_buffer->bytes_per_pixel;

  offset_buffer->bitmap_memory_size = offset_buffer->pitch*offset_buffer->height;

  if (offset_buffer->memory) {
    VirtualFree(offset_buffer->memory, 0, MEM_RELEASE);
  }

  offset_buffer->memory = VirtualAlloc(NULL, offset_buffer->bitmap_memory_size, 
                                       MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}


INTERNAL void win32_display_buffer_in_window(HDC                       device_ctx,
                                             win32_offscreen_buffer_t *offset_buffer,
                                             int                       width,
                                             int                       height)
{
  StretchDIBits(device_ctx,
                0, 0, offset_buffer->width, offset_buffer->height,
                0, 0, width, height,
                offset_buffer->memory,
                &offset_buffer->info,
                DIB_RGB_COLORS,
                SRCCOPY);
}


// ------- //
//  CLOCK  //
// ------- //

game_time_t platform_get_clock(void)
{
  game_time_t result;
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);

  result = (u64)(counter.QuadPart / win32_performence_frequency);

  return result;
}


// ------- //
//  SOUND  //
// ------- //

INTERNAL void win32_fill_sound_buffer(game_sound_output_t *game_sound_output,
                                      DWORD                byte_to_lock,
                                      DWORD                bytes_to_write)
{
  VOID *region[2];
  DWORD region_size[2];
  if (SUCCEEDED(win32_sound_output.buffer->lpVtbl->Lock(win32_sound_output.buffer, byte_to_lock, bytes_to_write,
                                                        &region[0], &region_size[0],
                                                        &region[1], &region_size[1],
                                                        0))) {
    s16 *sample_in = game_sound_output->samples;
    for (u32 reg_idx = 0; reg_idx < 2; ++reg_idx) {

      s16 *sample_out = (s16 *)region[reg_idx];
      DWORD region_sample_count = region_size[reg_idx] / win32_sound_output.bytes_per_sample;

      for (u32 sample_idx = 0; sample_idx < region_sample_count; ++sample_idx) {

        sample_out[0] = sample_in[0];
        sample_out[1] = sample_in[1];

        sample_out += 2;
        sample_in  += 2;

      }

      win32_sound_output.cur_sample += region_sample_count;
    }

    win32_sound_output.buffer->lpVtbl->Unlock(win32_sound_output.buffer,
                                              region[0], region_size[0],
                                              region[1], region_size[1]);
  }
  else {
    // TODO
  }
}


/***************************************************************************/
/* Sound:                                                                  */
/*                                                                         */
/* when we wake up to write audio:                                         */
/* look at the play cursor                                                 */
/* predict where it will be at the next frame boundry.                     */
/*                                                                         */
/* check if the write cursor is before that by at least the safety margin. */
/* define safety margin as the number of samples we think our              */
/* game update loop may vary by (half a frame).                            */
/*                                                                         */
/* if (write < predicted play - safety) (low latency)                      */
/*   target cursor is predicted play + 1                                   */
/* else (high latency)                                                     */
/*   target position is 1 frame + safety margin + extra margin             */
/***************************************************************************/
INTERNAL void win32_play_sound(game_time_t cur_time)
{
  LOCAL_PERSIST DWORD  play_cursor            = 0;
  LOCAL_PERSIST DWORD  write_cursor           = 0;
  LOCAL_PERSIST DWORD  audio_latency_bytes    = 0;
  LOCAL_PERSIST DWORD  exp_bytes_to_next_flip = 0;
  LOCAL_PERSIST r64    audio_latency_sec      = 0;
  LOCAL_PERSIST b32    sound_is_valid         = FALSE;

#ifdef GAME_INTERNAL
#ifdef SOUND_MARKERS
  LOCAL_PERSIST win32_debug_sound_marker_t sound_markers[GAME_UPDATE_HZ/2] = { 0 };
  LOCAL_PERSIST u32 sound_marker_idx = 0;
#endif
#endif

  DWORD byte_to_lock    = 0;
  DWORD target_cursor   = 0;
  DWORD bytes_to_write  = 0;

  if (SUCCEEDED(win32_sound_output.buffer->lpVtbl->GetCurrentPosition(win32_sound_output.buffer,
                                                                      &play_cursor,
                                                                      &write_cursor))) {
    u32 buffer_size      = win32_sound_output.buffer_size; 
    u32 samples_per_sec  = win32_sound_output.samples_per_sec;
    u32 bytes_per_sample = win32_sound_output.bytes_per_sample;

    if (!sound_is_valid) {
      win32_sound_output.cur_sample = write_cursor / bytes_per_sample;
      sound_is_valid = TRUE;
    }

    byte_to_lock = (win32_sound_output.cur_sample * bytes_per_sample) % buffer_size;


    DWORD exp_sound_bytes_per_frame = samples_per_sec*bytes_per_sample/SOUND_UPDATE_HZ;

    game_time_t sound_time = platform_get_clock();
    r32 flip_to_sound_sec = USEC2SEC(sound_time - cur_time);
    r32 sound_to_next_flip = SOUND_LATENCY - flip_to_sound_sec;

    exp_bytes_to_next_flip = (DWORD)(sound_to_next_flip / SOUND_LATENCY * (r32)exp_sound_bytes_per_frame);

    DWORD exp_frame_bound_byte = play_cursor + exp_sound_bytes_per_frame;

    // unwrap write_cursor
    if (write_cursor < play_cursor) write_cursor += buffer_size;
    ASSERT(write_cursor >= play_cursor);

    // low latency
    if (write_cursor + win32_sound_output.safety_samples_bytes < exp_frame_bound_byte) {
      target_cursor = exp_frame_bound_byte + exp_sound_bytes_per_frame;
    }
    // high latency
    else {
      target_cursor = write_cursor + exp_sound_bytes_per_frame + win32_sound_output.safety_samples_bytes;
    }

    // wrap target cursor
    target_cursor %= buffer_size;

    // compute bytes to write
    if (byte_to_lock <= target_cursor)
      bytes_to_write = target_cursor - byte_to_lock;
    else
      bytes_to_write = buffer_size + target_cursor - byte_to_lock;
  }
  else {
    sound_is_valid = FALSE;
  }

  u32 samples_count = bytes_to_write / win32_sound_output.bytes_per_sample;
  ASSERT(samples_count <= 48000);

  game_sound_output_t game_sound_output = {
    .samples_per_sec = win32_sound_output.samples_per_sec,
    .samples_count   = samples_count,
    .samples         = win32_sound_output.tmp_sound_buffer,
  };

  game_get_sound_samples_f(&game_sound_output);

#ifdef GAME_INTERNAL
#ifdef SOUND_MARKERS
  sound_markers[sound_marker_idx].output_next_play_cursor = exp_bytes_to_next_flip;
  sound_markers[sound_marker_idx].output_play_cursor   = play_cursor;
  sound_markers[sound_marker_idx].output_write_cursor  = write_cursor;
  sound_markers[sound_marker_idx].output_location      = byte_to_lock;
  sound_markers[sound_marker_idx].output_byte_count    = bytes_to_write;

  if (++sound_marker_idx == GAME_UPDATE_HZ/2)
    sound_marker_idx = 0;
    
  DWORD unwrapped_write_cursor = write_cursor;
  if (unwrapped_write_cursor < play_cursor)
    unwrapped_write_cursor += win32_sound_output.buffer_size;

  audio_latency_bytes = unwrapped_write_cursor - play_cursor;
  audio_latency_sec = ((r64)audio_latency_bytes              /
                        win32_sound_output.bytes_per_sample) /
                        win32_sound_output.samples_per_sec;

  sprintf_s(str, sizeof(str), "BTL:%u TC:%u BTW %u - PC:%u WC:%u DELTA:%u LAT:%fs\n",
            byte_to_lock, target_cursor, bytes_to_write,
            play_cursor, write_cursor, audio_latency_bytes, audio_latency_sec);
  //OutputDebugStringA(str);
#endif
#endif

  if (sound_is_valid) {
    win32_fill_sound_buffer(&game_sound_output,
                            byte_to_lock,
                            bytes_to_write);
  }


#ifdef GAME_INTERNAL
#ifdef SOUND_MARKERS
  win32_DEBUG_sound_sync(&win32_offscreen_buffer,
                         sound_markers,
                         GAME_UPDATE_HZ/2,
                         sound_marker_idx,
                         &win32_sound_output,
                         SECONDS_PER_FRAME);

  win32_sound_output.buffer->lpVtbl->GetCurrentPosition(win32_sound_output.buffer,
      &play_cursor,
      &write_cursor);

  sound_markers[sound_marker_idx].flip_play_cursor  = play_cursor;
  sound_markers[sound_marker_idx].flip_write_cursor = write_cursor;
#endif
#endif
}


// -------- //
//  Events  //
// -------- //

INTERNAL LRESULT CALLBACK win32_main_window_callback(HWND   window,
                                                     UINT   message,
                                                     WPARAM wparam,
                                                     LPARAM lparam)
{
  switch (message) {
    case WM_SIZE: {
      char str[256];
      win32_window_dimension_t window_dimension = win32_get_window_dimension(window);
      wsprintf(str, "WM_SIZE %d,%d\n", window_dimension.width, window_dimension.height);
      OutputDebugStringA(str);

      win32_state.resize = TRUE;
    } break;

    case WM_DESTROY: {
      OutputDebugStringA("WM_DESTROY\n");

      win32_state.running = FALSE;
    } break;

    case WM_CLOSE: {
      OutputDebugStringA("WM_CLOSE\n");

      win32_state.running = FALSE;
    } break;

    case WM_ACTIVATEAPP: {
      OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC device_ctx = BeginPaint(window, &paint);

      EndPaint(window, &paint);
    } break;

    default: {
      // OutputDebugStringA("default\n");
      return DefWindowProc(window, message, wparam, lparam);
    } break;
  }
  return 0;
}


// -------- //
//  Inputs  //
// -------- //

INTERNAL void win32_read_input(win32_input_t *win32_input)
{
  MSG message;

  while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {

    switch (message.message) {
      case WM_QUIT: {
        win32_state.running = FALSE;
        continue;
      } break;

      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP: {
        u32 vk_code = (u32)message.wParam;
        u8 press_rel = (message.lParam & (1 << 31)) == 0;

        for (s32 key = 0; key < LOGIC_KEYS_N; ++key) {
          if (win32_input->vk_code[key] == vk_code)
            win32_input->press_rel[key] = press_rel;
        }

        if (vk_code == VK_F4 && (message.lParam & (1 << 29)) != 0) {
          win32_state.running = FALSE;
        }

        continue;
      } break;

      default: {
        TranslateMessage(&message);
        DispatchMessage(&message);
      } break;
    }
  }
}


// -------- //
//  Reload  //
// -------- //

INTERNAL void win32_reload_game(game_memory_t *game_memory)
{
  LOCAL_PERSIST b32 first_load = TRUE;

  LOCAL_PERSIST HMODULE game_lib;

  LOCAL_PERSIST char exe_file_name[MAX_PATH];
  char relative_src_dll_name[] = "game.dll";
  char relative_tmp_dll_name[] = "hot_game.dll";
  char src_dll_name[MAX_PATH];
  char tmp_dll_name[MAX_PATH];
  
  LOCAL_PERSIST DWORD exe_file_name_len = 0;
  if (!exe_file_name_len)
    exe_file_name_len = GetModuleFileNameA(0, exe_file_name, sizeof(exe_file_name));

  char *file_pos = exe_file_name;
  for (char *p = exe_file_name; *p && p - exe_file_name < MAX_PATH; ++p)
    if (*p == '\\')
      file_pos = p+1;

  win32_string_cat(exe_file_name, (u32)(file_pos - exe_file_name),
                   relative_src_dll_name, sizeof(relative_src_dll_name),
                   src_dll_name, sizeof(src_dll_name));

  win32_string_cat(exe_file_name, (u32)(file_pos - exe_file_name),
                   relative_tmp_dll_name, sizeof(relative_tmp_dll_name),
                   tmp_dll_name, sizeof(tmp_dll_name));

  LOCAL_PERSIST b32 just_reloaded = FALSE;    
  b32 reload = FALSE;

  LOCAL_PERSIST FILETIME last_write_time;
  FILETIME cur_write_time = win32_get_last_write_time(src_dll_name);
  if (CompareFileTime(&cur_write_time, &last_write_time)) {
    if (!just_reloaded) {
      reload = TRUE;
      just_reloaded = TRUE;
    }
  }

  if (just_reloaded && !reload)
    just_reloaded = FALSE;

  last_write_time = cur_write_time;

  if (!reload)
    return;


  if (game_lib)
    FreeLibrary(game_lib);

  CopyFile(src_dll_name, tmp_dll_name, FALSE);

  game_lib = LoadLibraryA(tmp_dll_name);

  if (game_lib) {
    s32 err = 0;

    game_set_platform_f = (game_set_platform_t *)GetProcAddress(game_lib, "game_set_platform");
    platform_reload_game(game_set_platform_f);

#if 0
    for (u64 i = 0; i < game_memory->memory_size; ++i)
      game_memory->memory[i] = 0x00;
#endif

    game_init_f(game_memory, first_load);
    first_load = FALSE;
  }
}


// ------ //
//  MAIN  //
// ------ //

int CALLBACK WinMain(HINSTANCE Instance,
                     HINSTANCE PrevInstance,
                     LPSTR lpCmdLine,
                     int nCmdShow)
{
  char str[256];

  win32_set_query_performence_frequency();

  UINT scheduler_ms = 1;
  b32 sleep_is_granular = timeBeginPeriod(scheduler_ms) == TIMERR_NOERROR;

  WNDCLASS window_class = { 0 };

  win32_resize_DIB_section(&win32_offscreen_buffer, 1280, 720);

  window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = win32_main_window_callback;
  window_class.hInstance = Instance;
  window_class.lpszClassName = "Game";

  if (RegisterClass(&window_class)) {
    HWND window = CreateWindowEx(0, window_class.lpszClassName, "Game",
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 0, 0, Instance, 0);

    if (window) {

      win32_init_dsound(window, 48000, 2);
      win32_clear_sound_buffer();
      win32_sound_output.buffer->lpVtbl->Play(win32_sound_output.buffer, 0, 0, DSBPLAY_LOOPING);

      PAINTSTRUCT paint;
      HDC device_ctx = BeginPaint(window, &paint);
      EndPaint(window, &paint);

      LPVOID base_address = 0;
#ifdef GAME_INTERNAL
      base_address = (LPVOID)0x0000002000000000;
#endif

      game_memory_t game_memory = { 0 };
      game_memory.memory_size = MB(512);
      game_memory.memory = VirtualAlloc(base_address, game_memory.memory_size, 
                                        MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

      win32_sound_output.tmp_sound_buffer = VirtualAlloc(NULL, 4*48000, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

      win32_input_t win32_input = { 0 };
      win32_init_input(&win32_input);

      win32_state.running = TRUE;

      game_time_t frame_time = platform_get_clock();
      game_time_t cur_time = frame_time;
      u64 begin_rdtsc = __rdtsc();

      // game loop
      while (win32_state.running) {

        // Reload
        win32_reload_game(&game_memory);

        // Resize
        if (win32_state.resize) {
          win32_state.resize = FALSE;
          win32_window_dimension_t window_dimension = win32_get_window_dimension(window);
          win32_resize_DIB_section(&win32_offscreen_buffer,
                                    window_dimension.width,
                                    window_dimension.height);
        }

        // Game Update & Render
        game_offscreen_buffer_t game_offscreen_buffer = {
          .memory             = win32_offscreen_buffer.memory,
          .width              = win32_offscreen_buffer.width,
          .height             = win32_offscreen_buffer.height,
          .bytes_per_pixel    = win32_offscreen_buffer.bytes_per_pixel,
          .pitch              = win32_offscreen_buffer.pitch,
          .bitmap_memory_size = win32_offscreen_buffer.bitmap_memory_size,
        };

        win32_read_input(&win32_input);

        game_input_t game_input;
        for (s32 key = 0; key < LOGIC_KEYS_N; ++key)
          game_input.keys[key].press_rel = win32_input.press_rel[key];

        game_update_and_render_f(&game_offscreen_buffer,
                                 &game_input,
                                  SECONDS_PER_FRAME);

        // Exit from Game
        game_state_t *game_state = (game_state_t *)game_memory.memory;
        if (!game_state->running) {
          win32_state.running = FALSE;
          break;
        }

        // Sound
        win32_play_sound(cur_time);

        // Video
        win32_window_dimension_t win_dim = win32_get_window_dimension(window);
        win32_display_buffer_in_window(device_ctx,
                                       &win32_offscreen_buffer,
                                       win_dim.width,
                                       win_dim.height);

        // Sleep
        u64 end_rdtsc = __rdtsc();
        u64 cycles_diff  = end_rdtsc - begin_rdtsc;
        u32 m_cycles_diff = (u32)(cycles_diff / (1000*1000));

        do {
          cur_time = platform_get_clock();

          DWORD sleep_ms = (u32)USEC2MSEC(USECONDS_PER_FRAME - cur_time + frame_time);
          if ((s32)sleep_ms <= 0)
            break;

          if (sleep_is_granular) {
            wsprintf(str, "sleep for %dms\n", sleep_ms);
            //OutputDebugStringA(str);
            Sleep(sleep_ms);
          }
        } while (cur_time - frame_time < USECONDS_PER_FRAME);

        wsprintf(str, "time_diff:(%dus, %dMcyc)\n", cur_time - frame_time, m_cycles_diff);
        //OutputDebugStringA(str);

        begin_rdtsc = end_rdtsc;
        frame_time = cur_time;
      }
    }
    else {
      // TODO
    }
  }
  else {
    // TODO
  }

  return 0;
}


  
