
GLOBAL_VARIABLE const struct png_magic_t {
  u32 magic_size;
  u8  magic[8];
} png_magic_gp = {
  .magic_size = 8,
  .magic = {137, 'P', 'N', 'G', '\r', '\n', 26, '\n'},
};


typedef struct {
  u32 width;
  u32 height;
  u8  bit_depth;
  u8  color_type;
  u8  compression_method;
  u8  filter_method;
  u8  interlace_method;
} png_header_t;



s32 png_filter(u8 *buf, u32 buf_size, png_data_t *png_data)
{
  typedef enum {
    FILTER_NONE  = 0,
    FILTER_SUB   = 1,
    FILTER_UP    = 2,
    FILTER_AVG   = 3,
    FILTER_PAETH = 4,
  } filter_type_e ;

  // Z initialize first row to allow up ptr to point to it and act as having shadow zeros.
  for (u32 x = 0; x < png_data->width; ++x)
    png_data->pixels[x].out = 0;


  if (png_data->rgb_active)  {
    ASSERT(png_data->pixel_bytes == 3 ||
           png_data->pixel_bytes == 4);

    u32 alpha_mask  = ((u32)~0) >> (8*(png_data->alpha_active ^ 1));
    u32 pixel_bytes = png_data->pixel_bytes;

    png_pixel_t cur;
    png_pixel_t left;
    png_pixel_t *up = png_data->pixels;
    png_pixel_t leftup;

    u8      *src_line = buf;
    png_pixel_t *out_line = png_data->pixels;
    for (u32 y = 0; y < png_data->height; ++y) {


      filter_type_e filter_type = src_line[0];

      left.out = leftup.out = 0;

      u8      *src = &src_line[1];
      png_pixel_t *out = out_line;

      switch (filter_type) {
        case FILTER_NONE: {
              for (u32 x = 0; x < png_data->width; ++x) {

            cur.out = *(u32 *)src & alpha_mask;

            *out++ = cur;
            src += pixel_bytes;
          }
        } break;

        case FILTER_SUB: {
          for (u32 x = 0; x < png_data->width; ++x) {

            cur.out = *(u32 *)src & alpha_mask;

            cur.channels[0] += left.channels[0];
            cur.channels[1] += left.channels[1];
            cur.channels[2] += left.channels[2];
            cur.channels[3] += left.channels[3];

            *out++ = cur;
            src += pixel_bytes;

            left = cur;
          }
        } break;

        case FILTER_UP: {
          for (u32 x = 0; x < png_data->width; ++x) {

            cur.out = *(u32 *)src & alpha_mask;

            cur.channels[0] += up->channels[0];
            cur.channels[1] += up->channels[1];
            cur.channels[2] += up->channels[2];
            cur.channels[3] += up->channels[3];

            *out++ = cur;
            src += pixel_bytes;

            up++;
          }
        } break;

        case FILTER_AVG: {
          for (u32 x = 0; x < png_data->width; ++x) {

            cur.out = *(u32 *)src & alpha_mask;

            png_pixel_t avg;
            avg.channels[0] = ((u16)left.channels[0] + (u16)up->channels[0]) >> 1;
            avg.channels[1] = ((u16)left.channels[1] + (u16)up->channels[1]) >> 1;
            avg.channels[2] = ((u16)left.channels[2] + (u16)up->channels[2]) >> 1;
            avg.channels[3] = ((u16)left.channels[3] + (u16)up->channels[3]) >> 1;

            cur.channels[0] += avg.channels[0];
            cur.channels[1] += avg.channels[1];
            cur.channels[2] += avg.channels[2];
            cur.channels[3] += avg.channels[3];

            *out++ = cur;
            src += pixel_bytes;

            left = cur;
            up++;
          }
        } break;

        case FILTER_PAETH: {
          for (u32 x = 0; x < png_data->width; ++x) {

            cur.out = *(u32 *)src & alpha_mask;

            png_pixel_t paeth;
            for (u32 c = 0; c < 4; ++c) {
              s16 p   = (s16)left.channels[c] + (s16)up->channels[c] - (s16)leftup.channels[c];
              s16 pl  = (s16)p - (s16)left.channels[c];   if (pl < 0)  pl = -pl;
              s16 pu  = (s16)p - (s16)up->channels[c];    if (pu < 0)  pu = -pu;
              s16 plu = (s16)p - (s16)leftup.channels[c]; if (plu < 0) plu = -plu;

              if (pl <= pu && pl <= plu) {
                paeth.channels[c] = left.channels[c];
              }
              else {
                if (pu <= plu) {
                  paeth.channels[c] = up->channels[c];
                }
                else {
                  paeth.channels[c] = leftup.channels[c];
                }
              }
            }

            cur.channels[0] += paeth.channels[0];
            cur.channels[1] += paeth.channels[1];
            cur.channels[2] += paeth.channels[2];
            cur.channels[3] += paeth.channels[3];

            *out++ = cur;
            src += pixel_bytes;

            left   = cur;
            leftup = *up++;
          }
        } break;

        default: {
          ASSERT(0);
        } break;
      }

      up = out_line;

      src_line += png_data->stride;
      out_line += png_data->width;
    }
    
    return 0;
  }

  if (png_data->greyscale_active)  {
    
    return 0;
  }

  if (png_data->pallete_active)  {
    
    return 0;
  }

  return 0;
}


s32 png_parse(u8 *png_file_buf, u32 png_file_size, png_data_t *png_data)
{
  s32 err = 0;

  for (u8 i = 0; i < png_magic_gp.magic_size; ++i)
    if (png_file_buf[i] != png_magic_gp.magic[i]) return -1;

  u8 *reader = &png_file_buf[png_magic_gp.magic_size];

  png_header_t header = { 0 };
  u8 *uncomp_buf  = NULL;
  u32 uncomp_size = 0;

  struct {
    u32 len;
    u32 type;
    u32 crc;
  } chunk = { 0 };

  u8 *comp_buf  = arena_alloc(&png_data->arena, 0);
  u32 comp_size = 0;
  u32 idat_idx  = 0;

  enum {
    PNG_IHDR = 'RDHI',
    PNG_PLTE = 'ETLP',
    PNG_IDAT = 'TADI',
    PNG_IEND = 'DNEI',
    PNG_tRNS = 'SNRt',
    PNG_gAMA = 'AMAg',
    PNG_sRGB = 'BGRs',
    PNG_pHYs = 'sYHp',
  };

  do {
    if ((u32)(reader - png_file_buf) + 8 > png_file_size) return -1;

    chunk.len = math_endian_swap_u32(*(u32*)reader);
    chunk.type = *(u32*)&reader[4];
    if ((u32)(reader - png_file_buf) + chunk.len + 12 > png_file_size) return -1;
    reader += 8;

    switch (chunk.type) {

      case PNG_IHDR: {

        header = *(png_header_t*)reader;

        if (header.filter_method != 0) return -1;

        png_data->width  = math_endian_swap_u32(header.width);
        png_data->height = math_endian_swap_u32(header.height);
        ASSERT(png_data->width > 0 && png_data->height > 0);

        png_data->alpha_active = (header.color_type & 4) >> 2;

        switch (header.color_type & 3) {
          case 0: {
            png_data->greyscale_active = TRUE;
          } break;
          case 2: {
            png_data->rgb_active = TRUE;
          } break;
          case 3: {
            png_data->pallete_active = TRUE;
          } break;
          default: {
            return -1;
          } break;
        }

        if (header.color_type > 6) return -1;

        if (png_data->rgb_active) {
          ASSERT(header.bit_depth == 8 ||
                 header.bit_depth == 16);

          u32 bpc = header.bit_depth >> 3;
          png_data->pixel_bytes = bpc * (3 + png_data->alpha_active);
        }

        if (png_data->greyscale_active) {
          if (png_data->alpha_active) {
            ASSERT(header.bit_depth == 8 ||
                   header.bit_depth == 16);

            u32 bpc = header.bit_depth >> 3;
            png_data->pixel_bytes = bpc * 2;
          }
          else {
            ASSERT(header.bit_depth == 1 ||
                   header.bit_depth == 2 ||
                   header.bit_depth == 4 ||
                   header.bit_depth == 8 ||
                   header.bit_depth == 16);

            png_data->sizes_in_bits = TRUE;
          }
        }

        if (png_data->pallete_active) {
          ASSERT(header.bit_depth == 1 ||
                 header.bit_depth == 2 ||
                 header.bit_depth == 4 ||
                 header.bit_depth == 8 ||
                 header.bit_depth == 16);
          ASSERT(!png_data->alpha_active);

          png_data->sizes_in_bits = TRUE;
        }

        if (png_data->sizes_in_bits) {
          png_data->pixel_bits = header.bit_depth;
          png_data->stride = ((png_data->width * png_data->pixel_bits + 7) >> 3) + 1;
        }
        else
          png_data->stride = png_data->width * png_data->pixel_bytes + 1;

        uncomp_size = png_data->stride * png_data->height;

      } break;

      case PNG_PLTE: {
      } break;

      case PNG_IDAT: {

        if (!uncomp_size) return -1;
        
        // ghost allocate chunks to "add" to the compressed buffer
        arena_alloc(&png_data->arena, chunk.len);
        for (u32 i = 0; i < chunk.len; ++i)
          comp_buf[comp_size+i] = reader[i];

        comp_size += chunk.len;
        ++idat_idx;

      } break;

      case PNG_IEND: {
      } break;

      case PNG_tRNS: {
      } break;

      case PNG_gAMA: {
      } break;

      case PNG_sRGB: {
      } break;

      case PNG_pHYs: {
      } break;

      default: {
      } break;
    }

    reader += chunk.len;
    chunk.crc = math_endian_swap_u32(*(u32*)reader);
    reader += 4;
  } while (chunk.type != PNG_IEND);

  uncomp_buf = arena_alloc(&png_data->arena, uncomp_size);
  err = zlib_deflate((u16*)comp_buf, comp_size,
                     uncomp_buf, uncomp_size,
                     &png_data->arena);
  if (err) return err;

  png_data->pixels = arena_alloc(&png_data->arena, sizeof(png_pixel_t) * png_data->width * png_data->height);
  err = png_filter(uncomp_buf, uncomp_size, png_data);
  if (err) return err;
  
  // PNG is in RGBA and natively we work at BGRA
  for (u32 i = 0; i < png_data->width * png_data->height; ++i) {
    u8 tmp = png_data->pixels[i].r;
    png_data->pixels[i].r = png_data->pixels[i].b;
    png_data->pixels[i].b = tmp;
  }

  return err;
}


b32 png_test(game_offscreen_buffer_t *offscreen_buffer)
{
  LOCAL_PERSIST u32 iter;
  ++iter;
  if (iter > 40)
    iter = 0;

  if (iter < 20)
    return FALSE;

  if (iter == 20) {

    u32 png_file_size = 0;
    u8 *png_file_buf = NULL;

    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\test.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\test_uncomp.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_uncomp.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_1.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_2.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_3.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_4.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_5.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_6.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_7.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_8.png", &png_file_size);
    //png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\casey_comp_9.png", &png_file_size);
    png_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\shalom.png", &png_file_size);
    ASSERT(png_file_buf != NULL);

#define PNG_MAX_SIZE MB(64)
    LOCAL_PERSIST u8 png_buf[PNG_MAX_SIZE];
    png_data_t png_data = { 0 };
    png_data.arena = arena_init(png_buf, PNG_MAX_SIZE);

    s32 err = png_parse(png_file_buf, png_file_size, &png_data);
    ASSERT(!err);

    platform_DEBUG_free_file_memory_f(png_file_buf);

    r32 ratio_h = (r32)png_data.height / (r32)offscreen_buffer->height;
    r32 ratio_w = (r32)png_data.width  / (r32)offscreen_buffer->width;

    u8 *row = (u8 *)offscreen_buffer->memory;
    for (s32 y = 0; y < offscreen_buffer->height; ++y) {
      u32 *pixel = (u32 *)row;
      for (s32 x = 0; x < offscreen_buffer->width; ++x) {
        u32 png_y = (u32)((offscreen_buffer->height - y - 1) * ratio_h);
        u32 png_x = (u32)(x * ratio_w);

        *pixel = png_data.pixels[png_y*png_data.width+png_x].out;

        ++pixel;
      }

      row += offscreen_buffer->pitch;
    }
  }

  return TRUE;
}


