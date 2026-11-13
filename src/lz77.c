

#define N_CLEN 19
#define N_LITERAL 287
#define N_DIST 31

#define HUFF_MAX_BIT_LENGTH 16
#define HUFF_TAB_SIZE (1<<HUFF_MAX_BIT_LENGTH)

#define HUFF_EMPTY_ENTRY 65532


s32 zlib_parse_header(u16 *buf, u32 buf_size, u32 *buf_pos)
{
  typedef struct {
    union {
      struct {
        union {
          struct {
            u8 cmethod:4;
            u8 cinfo:4;
          };
          u8 cmf;
        };
        union {
          struct {
            u8 fcheck:5;
            u8 fdict:1;
            u8 flevel:2;
          } flags;
          u8 flags_u8;
        };
      };
      u16 bits;
    };
  } zlib_header_t;

  zlib_header_t zlib_header = (zlib_header_t) { .bits = math_bits_read_16(buf, buf_pos, 16) };
  if ((u32)zlib_header.cmethod != 8)
    return -1;

  if (zlib_header.cinfo > 7)
    return -1;

  // u32 window_size = 1 << (zlib_header.cinfo+8);

  u32 check_value = (zlib_header.cmf<<8) + zlib_header.flags_u8;
  if (check_value % 31 > 0)
    return -1;

  if (zlib_header.flags.fdict != 0)
    return -1;

  return 0;
}


s32 zlib_huffman_build(u16 *bit_lens, u16 n_syms, u16 *huffman_tab)
{
  for (u32 i = 0; i < HUFF_TAB_SIZE; ++i)
    huffman_tab[i] = HUFF_EMPTY_ENTRY;

  u16 bl_count[HUFF_MAX_BIT_LENGTH]    = { 0 };
  u16 bl_code[HUFF_MAX_BIT_LENGTH]     = { 0 };
  u16 bl_max_code[HUFF_MAX_BIT_LENGTH] = { 0 };

  for (u16 sym = 0; sym < n_syms; ++sym) {
    if (bit_lens[sym] >= HUFF_MAX_BIT_LENGTH) return -1;

    ++bl_count[bit_lens[sym]];
  }

  bl_count[0] = 0;
  
  for (u16 bit_idx = 1, code = 0; bit_idx < HUFF_MAX_BIT_LENGTH; ++bit_idx) {

    bl_code[bit_idx]     = bl_max_code[bit_idx-1] << 1;
    bl_max_code[bit_idx] = bl_code[bit_idx] + bl_count[bit_idx];

    if (bl_max_code[bit_idx] > ((u16)1 << bit_idx)) return -1;
  }

  for (u16 sym = 0; sym < n_syms; ++sym) {
    u16 sym_len = bit_lens[sym];
    if (sym_len) {

      u16 code = bl_code[sym_len]++;
      ASSERT(code <= bl_max_code[sym_len]);

      code = math_mirror_u16(code, sym_len);

      u16 x_code = code;
      u16 step = 1 << sym_len;
      u32 x_bits = (u32)1 << (HUFF_MAX_BIT_LENGTH - sym_len);
      ASSERT((u32)step * (x_bits-1) + code < HUFF_TAB_SIZE);
      while (x_bits--) {
        ASSERT(huffman_tab[x_code] == HUFF_EMPTY_ENTRY);
        huffman_tab[x_code] = sym;
        x_code += step;
      }
    }
  }

  return 0;
}


s32 zlib_huffman_deflate_dynamic(u16 *buf, u32 buf_size, u32 *buf_pos,
                                 u16 *lit_huffman_tab , u16 *dist_huffman_tab, u16 *clen_huffman_tab,
                                 u16 *lit_dist_lengths, u16 *n_lit)
{
  s32 err = 0;

  // read the huffman codes for literal/length + distance.
  //
  // convert the data using the following huffman codes:
  // 0 - 15: Represent code lengths of 0 - 15
  // 16: Copy the previous code length 3 - 6 times.
  //     The next 2 bits indicate repeat length (0 = 3, ... , 3 = 6)
  //        Example:  Codes 8, 16 (+2 bits 11),
  //                  16 (+2 bits 10) will expand to
  //                  12 code lengths of 8 (1 + 6 + 5)
  // 17: Repeat a code length of 0 for 3 - 10 times.
  //     (3 bits of length)
  // 18: Repeat a code length of 0 for 11 - 138 times (7 bits of length)
  //
  // the result is a list of bit-lengths that reproduce the actual two huffman code trees.

  typedef struct {
    u16 hlit;
    u16 hdist;
    u16 hclen;
  } huffman_header_t;

  huffman_header_t header = { 0 };
  header.hlit  = math_bits_read_16(buf, buf_pos, 5) + 257;
  header.hdist = math_bits_read_16(buf, buf_pos, 5) + 1;
  header.hclen = math_bits_read_16(buf, buf_pos, 4) + 4;

  if (header.hlit  > N_LITERAL) return -1;
  if (header.hdist > N_DIST)    return -1;
  if (header.hclen > N_CLEN)    return -1;

  // read code lengths for the clen alphabet (yeah its wierd)
  u16 clen_symbols[N_CLEN] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
  u16 clen_lens[N_CLEN] = { 0 };
  for (u16 clen = 0; clen < header.hclen; ++clen)
    clen_lens[clen_symbols[clen]] = math_bits_read_16(buf, buf_pos, 3);

  zlib_huffman_build(clen_lens, N_CLEN, clen_huffman_tab);

  u16 n_lits_dist = header.hlit + header.hdist;

  for (u16 clen_idx = 0; clen_idx < n_lits_dist;) {
    u16 clen = clen_huffman_tab[math_bits_peek_16(buf, *buf_pos)];
    if (clen >= N_CLEN) return -1;
    *buf_pos += clen_lens[clen];

    if (clen < 16) {
      lit_dist_lengths[clen_idx++] = clen;
      continue;
    }
    
    u16 rep = 1;
    switch (clen) {

      case 16: {
        if (clen_idx == 0) return -1;

        rep = math_bits_read_16(buf, buf_pos, 2) + 3;
        if (rep > 6) return -1;

        clen = lit_dist_lengths[clen_idx-1];
      } break;

      case 17: {
        rep = math_bits_read_16(buf, buf_pos, 3) + 3;
        if (rep > 10) return -1;

        clen = 0;
      } break;

      case 18: {
        rep = math_bits_read_16(buf, buf_pos, 7) + 11;
        if (rep > 138) return -1;

        clen = 0;
      } break;

      default: {
      } break;
    }
    if (clen_idx+rep > n_lits_dist) return -1;

    u16 *dst = &lit_dist_lengths[clen_idx];
    clen_idx += rep;
    while (rep--)
      *(dst++) = clen;
  }

  u16 *lit_lengths  = &lit_dist_lengths[0];
  *n_lit = header.hlit;

  zlib_huffman_build(lit_dist_lengths, header.hlit, lit_huffman_tab);

  zlib_huffman_build(&lit_dist_lengths[header.hlit], header.hdist, dist_huffman_tab);

  return err;
}


s32 zlib_deflate(u16 *buf, u32 buf_size, u8*uncomp_buf, u32 uncomp_size, arena_t *arena)
{

  s32 err = 0;

  u32 buf_pos = 0;
  err = zlib_parse_header(buf, buf_size, &buf_pos);
  if (err) return err;

  struct {
    union {
      struct {
        u8 bfinal:1;
        u8 btype:2;
      };
      u16 bits;
    };
  } block_header;

  arena_t huffman_arena = arena_sub(arena, 2*3*HUFF_TAB_SIZE + 2*(N_LITERAL+N_DIST));

  u32 uncomp_idx = 0;
  u32 uncomp_blocks = 0;
  do {
    block_header.bits = math_bits_read_16(buf, &buf_pos, 3);
    u16 *lit_huffman_tab  = NULL;
    u16 *dist_huffman_tab = NULL;
    u16 *lit_lengths  = NULL;
    u16 *dist_lengths = NULL;

    arena_reset(&huffman_arena);

    typedef enum {
      NO_COMP      = 0,
      COMP_HUF_FIX = 1,
      COMP_HUF_DYN = 2,
    } comp_type_e;

    switch (block_header.btype) {

      case NO_COMP: {

        buf_pos = (buf_pos+7) & ((u32)~7);
        u16 len  =  (math_bits_read_16(buf, &buf_pos, 16));
        u16 nlen = ~(math_bits_read_16(buf, &buf_pos, 16));
        if (nlen != len) return -1;
        if (!len) return -1;

        //len = math_endian_swap_u16(len);

        u8 *src = &((u8*)buf)[buf_pos>>3];
        u8 *dst = &uncomp_buf[uncomp_idx];
        uncomp_idx += len;
        buf_pos += len<<3;
        while (len--)
          *(dst++) = *(src++);

      } break;

      case COMP_HUF_FIX: {

        LOCAL_PERSIST b32 initialized = FALSE;
        LOCAL_PERSIST u16 fixed_lit_huffman_tab [HUFF_TAB_SIZE];
        LOCAL_PERSIST u16 fixed_dist_huffman_tab[HUFF_TAB_SIZE];
        LOCAL_PERSIST u16 fixed_lit_lengths [N_LITERAL];
        LOCAL_PERSIST u16 fixed_dist_lengths[N_DIST];

        if (!initialized) {
          u16 lit_sects[] = { 0, 144, 256, 280, 287 };
          u16 lit_sect_len[] = { 8, 9, 7, 8 };
          for (u16 lit_sect = 0; lit_sect < 4; ++lit_sect)
            for (u16 lit = lit_sects[lit_sect]; lit < lit_sects[lit_sect+1]; ++lit)
              fixed_lit_lengths[lit] = lit_sect_len[lit_sect];
          zlib_huffman_build(fixed_lit_lengths, N_LITERAL, fixed_lit_huffman_tab);

          for (u16 dist = 0; dist < N_DIST; ++dist)
            fixed_dist_lengths[dist] = 5;
          zlib_huffman_build(fixed_dist_lengths, N_DIST, fixed_dist_huffman_tab);

          initialized = TRUE;
        }

        lit_huffman_tab  = fixed_lit_huffman_tab;
        dist_huffman_tab = fixed_dist_huffman_tab;
        lit_lengths  = fixed_lit_lengths;
        dist_lengths = fixed_dist_lengths;

      } break;

      case COMP_HUF_DYN: {
        u16 *lit_dist_lengths = arena_alloc(&huffman_arena, 2*(N_LITERAL+N_DIST));
        u16 *clen_huffman_tab = arena_alloc(&huffman_arena, 2*HUFF_TAB_SIZE);
        lit_huffman_tab       = arena_alloc(&huffman_arena, 2*HUFF_TAB_SIZE);
        dist_huffman_tab      = arena_alloc(&huffman_arena, 2*HUFF_TAB_SIZE);
        u16 n_lit = 0;

        err = zlib_huffman_deflate_dynamic(buf, buf_size, &buf_pos,
                                           lit_huffman_tab, dist_huffman_tab, clen_huffman_tab,
                                           lit_dist_lengths, &n_lit);
        if (err) return err;

        lit_lengths  =  lit_dist_lengths;
        dist_lengths = &lit_dist_lengths[n_lit]; 

      } break;

      default: {
        return -1;
      } break;
    }

    if (block_header.btype != NO_COMP) {

      for (;;) {
        if (uncomp_idx > uncomp_size) return -1;

        u16 lit = lit_huffman_tab[math_bits_peek_16(buf, buf_pos)];
        ASSERT(lit != HUFF_EMPTY_ENTRY);
        buf_pos += lit_lengths[lit];

        if (lit >= 286) return -1;

        if (lit < 256) {
          uncomp_buf[uncomp_idx++] = (u8)lit;
          continue;
        }

        if (lit == 256)
          break;

        // > 257 : length & distance

        u16 len_idx = lit - 257;
        
        LOCAL_PERSIST u16 len_ext_bits[29] = {
          0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
          1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
          4, 4, 4, 4, 5, 5, 5, 5, 0,
        };
        LOCAL_PERSIST u16 len_start[29] = {
           3,  4,  5,   6,   7,   8,   9,  10,  11, 13,
          15, 17, 19,  23,  27,  31,  35,  43,  51, 59,
          67, 83, 99, 115, 131, 163, 195, 227, 258,
        };
        u16 len = math_bits_read_16(buf, &buf_pos, len_ext_bits[len_idx]) + len_start[len_idx];

        u16 dist_idx = dist_huffman_tab[math_bits_peek_16(buf, buf_pos)];
        ASSERT(dist_idx != HUFF_EMPTY_ENTRY);
        if (dist_idx > 30) return -2;
        buf_pos += dist_lengths[dist_idx];

        LOCAL_PERSIST u16 dist_ext_bits[30] = {
          0, 0,  0,  0,  1,  1,  2,  2,  3,  3,
          4, 4,  5,  5,  6,  6,  7,  7,  8,  8,
          9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
        };
        LOCAL_PERSIST u16 dist_start[30] = {
             1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
            33,   49,   65,   97,  129,  193,  257,   385,   513,   769,
          1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
        };
        u16 dist = math_bits_read_16(buf, &buf_pos, dist_ext_bits[dist_idx]) + dist_start[dist_idx];

        if (uncomp_idx < dist)       return -3;
        if (uncomp_idx + len < dist) return -4;

        u8 *src = &uncomp_buf[uncomp_idx-dist];
        u8 *dst = &uncomp_buf[uncomp_idx];
        uncomp_idx += len;
        while (len--)
          *(dst++) = *(src++);
      }
    }

    ++uncomp_blocks;
  } while (!block_header.bfinal);

  if (uncomp_size != uncomp_idx) return -1;

  // adler32 checksum, not supported yet
  u32 adler32 = *(u32*)&((u8*)buf)[buf_size-4];
  adler32 = math_endian_swap_u32(adler32);

  u32 comp_adler32 = math_adler32(uncomp_buf, uncomp_size);

  if (comp_adler32 != adler32) return -1;

  return err;
}
