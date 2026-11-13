

#define MATH_IMPL(N, vec_t, mat_t)                                                   \
inline vec_t math_add_v##N(vec_t v1, vec_t v2)                                       \
{                                                                                    \
  vec_t v = { 0 }; for (u8 i = 0; i < N; ++i) v.v[i] = v1.v[i] + v2.v[i]; return v;  \
}                                                                                    \
                                                                                     \
inline vec_t math_sub_v##N(vec_t v1, vec_t v2)                                       \
{                                                                                    \
  vec_t v = { 0 }; for (u8 i = 0; i < N; ++i) v.v[i] = v1.v[i] - v2.v[i]; return v;  \
}                                                                                    \
                                                                                     \
inline vec_t math_mul_c_v##N(r32 c, vec_t v)                                      \
{                                                                                    \
  for (u8 i = 0; i < N; ++i) v.v[i] *= c; return v;                                  \
}                                                                                    \
                                                                                     \
inline r32 math_dot_prod_##N(vec_t v1, vec_t v2)                                  \
{                                                                                    \
  r32 s =  0 ; for (u8 i = 0; i < N; ++i) s += v1.v[i]*v2.v[i]; return s;         \
}                                                                                    \
                                                                                     \
inline r32 math_norm_v##N(vec_t v)                                                \
{                                                                                    \
  return sqrtf(math_dot_prod_##N(v, v));                                             \
}                                                                                    \
                                                                                     \
inline mat_t math_mul_mat_##N(mat_t m1, mat_t m2)                                    \
{                                                                                    \
  mat_t m = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) for (u8 j = 0; j < N; ++j) {                            \
    m.m[i][j] =  0 ; for (u8 k = 0; k < N; ++k) m.m[i][j] += m1.m[i][k]*m2.m[k][j];  \
  }                                                                                  \
  return m;                                                                          \
}                                                                                    \
                                                                                     \
inline vec_t math_mul_mat_vec_##N(mat_t m, vec_t v1)                                 \
{                                                                                    \
  vec_t v = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) {                                                       \
    v.v[i] =  0 ; for (u8 k = 0; k < N; ++k) v.v[i] += m.m[i][k]*v1.v[k];            \
  }                                                                                  \
  return v;                                                                          \
}                                                                                    \
                                                                                     \
inline vec_t math_mul_vec_mat_##N(vec_t v1, mat_t m)                                 \
{                                                                                    \
  vec_t v = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) {                                                       \
    v.v[i] =  0 ; for (u8 k = 0; k < N; ++k) v.v[i] += v1.v[k]*m.m[k][i];            \
  }                                                                                  \
  return v;                                                                          \
}                                                                                    \
                                                                                     \
inline mat_t math_trans_mat_##N(mat_t m1)                                            \
{                                                                                    \
  mat_t m = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) for (u8 j = 0; j < N; ++j) m.m[i][j] = m1.m[j][i];      \
  return m;                                                                          \
}
MATH_IMPL(2, vec2_t, mat2_t) MATH_IMPL(3, vec3_t, mat3_t) MATH_IMPL(4, vec4_t, mat4_t)


// corss product
inline vec3_t math_cross_prod(vec3_t v1, vec3_t v2)
{
  return (vec3_t) {
    .x = v1.y*v2.z - v1.z*v2.y,
    .y = v1.z*v2.x - v1.x*v2.z,
    .z = v1.x*v2.y - v1.y*v2.x,
  };
}


// triangle
vec3_t math_triangle_norm(triangle_t *triangle)
{
  vec3_t v = math_cross_prod(math_sub_v3(triangle->p1, triangle->p2),
                             math_sub_v3(triangle->p3, triangle->p2));
  return math_mul_c_v3(1.0F/math_norm_v3(v), v);
}


vec3_t math_triangle_center(triangle_t *triangle)
{
  return (vec3_t) {
    .x = (triangle->p1.x + triangle->p2.x + triangle->p3.x) / 3,
    .y = (triangle->p1.y + triangle->p2.y + triangle->p3.y) / 3,
    .z = (triangle->p1.z + triangle->p2.z + triangle->p3.z) / 3,
  };
}


void math_barycentric(r32 x, r32 y, vec2_t t[3], r32 l[3])
{
#define A 0
#define B 1
#define C 2
  r32 v0x = t[A].x - t[C].x;
  r32 v1x = t[B].x - t[C].x;
  r32 vpx =      x - t[C].x;

  r32 v0y = t[A].y - t[C].y;
  r32 v1y = t[B].y - t[C].y;
  r32 vpy =      y - t[C].y;

  r32 den = v0x*v1y - v1x*v0y;

  l[0] = (vpx*v1y - v1x*vpy) / den;
  l[1] = (v0x*vpy - vpx*v0y) / den;
  l[2] = 1 - l[0] - l[1];

  if (l[0] < -0.1 ||
      l[1] < -0.1 ||
      l[2] < -0.1 ||
      l[0] > 1.1  ||
      l[1] > 1.1  ||
      l[2] > 1.1) {
    l[0] = 1.0F/3;
    l[1] = 1.0F/3;
    l[2] = 1.0F/3;
  }

  if (l[0] < 0) l[0] = 0;
  if (l[0] > 1) l[0] = 1;
  if (l[1] < 0) l[1] = 0;
  if (l[1] > 1) l[1] = 1;
  if (l[2] < 0) l[2] = 0;
  if (l[2] > 1) l[2] = 1;

}


// euler angles
mat3_t math_roll_mat(r32 roll)
{
  r32 s = sinf(roll);
  r32 c = cosf(roll);
  return (mat3_t) { .m = {
    { 1,  0, 0 },
    { 0,  c, s },
    { 0, -s, c },
  }};
}


mat3_t math_pitch_mat(r32 pitch)
{
  r32 s = sinf(pitch);
  r32 c = cosf(pitch);
  return (mat3_t) { .m = {
    {  c, 0, s },
    {  0, 1, 0 },
    { -s, 0, c },
  }};
}


mat3_t math_yaw_mat(r32 yaw)
{
  r32 s = sinf(yaw);
  r32 c = cosf(yaw);
  return (mat3_t) { .m = {
    {  c, s, 0 },
    { -s, c, 0 },
    {  0, 0, 1 },
  }};
}


// quaternions
quaternion_t math_quaternion_mul(quaternion_t q1, quaternion_t q2)
{
  quaternion_t q = { 0 };
  q.w = q1.w*q2.w - q1.i*q2.i - q1.j*q2.j - q1.k*q2.k;
  q.i = q1.w*q2.i + q1.i*q2.w + q1.j*q2.k - q1.k*q2.j;
  q.j = q1.w*q2.j - q1.i*q2.k + q1.j*q2.w + q1.k*q2.i;
  q.k = q1.w*q2.k + q1.i*q2.j - q1.j*q2.i + q1.k*q2.w;
  return q;
}


quaternion_t math_quaternion(vec3_t axis, r32 angle)
{
  quaternion_t q = { 0 };
  r32 c = cosf(angle/2);
  r32 s = sinf(angle/2);
  q.v = math_mul_c_v3(s, axis);
  q.w = c;
  return q;
}


vec3_t math_quaternion_rotate(quaternion_t q, vec3_t v)
{
  quaternion_t p = {.v = v, .w = 0};
  p = math_quaternion_mul(q, p);
  q.i *= -1.0F; q.j *= -1.0F; q.k *= -1.0F;
  p = math_quaternion_mul(p, q);
  return p.v;
}


vec3_t math_quaternion_rotate_inv(quaternion_t q, vec3_t v)
{
  quaternion_t p = {.v = v, .w = 0};
  quaternion_t q_i = q;
  q_i.i *= -1.0F; q_i.j *= -1.0F; q_i.k *= -1.0F;
  p = math_quaternion_mul(q_i, p);
  p = math_quaternion_mul(p, q);
  return p.v;
}


/**************************************************************/
/*                                                            */
/* why quaternion multiplication works:                       */
/*                                                            */
/* (pq)-1 = q-1*p-1:                                          */
/*                                                            */
/* (pq)-1 =                                                   */
/* ((pw + pi*i + pj*j + pk*k) * (qw + qi*i + qj*j + qk*k))-1  */
/*  =                                                         */
/* ((pw*qw - pi*qi - pj*qj - pk*qk)   )                       */
/* ((pw*qi + pi*qw + pj*qk - pk*qj)*i )   -1                  */
/* ((pw*qj - pi*qk + pj*qw + pk*qi)*j )                       */
/* ((pw*qk + pi*qj - pj*qi + pk*qw)*k )                       */
/*  =                                                         */
/* (1)                                                        */
/* ( pw*qw - pi*qi - pj*qj - pk*qk)                           */
/* (-pw*qi - pi*qw - pj*qk + pk*qj)*i                         */
/* (-pw*qj + pi*qk - pj*qw - pk*qi)*j                         */
/* (-pw*qk - pi*qj + pj*qi - pk*qw)*k                         */
/*                                                            */
/* q-1*p-1                                                    */
/* (qw - qi*i - qj*j - qk*k) * (pw - pi*i - pj*j - pk*k)      */
/*  =                                                         */
/* ( qw*pw - qi*pi - qj*pj - qk*pk)                           */
/* (-qw*pi - qi*pw + qj*pk - qk*pj)*i                         */
/* (-qw*pj - qi*pk - qj*pw + qk*pi)*j                         */
/* (-qw*pk + qi*pj - qj*pi - qk*pw)*k                         */
/*  =                                                         */
/* (2)                                                        */
/* ( qw*pw - qi*pi - qj*pj - qk*pk)                           */
/* (-qi*pw - qw*pi - qk*pj + qj*pk)*i                         */
/* (-qj*pw + qk*pi - qw*pj - qi*pk)*j                         */
/* (-qk*pw - qj*pi + qi*pj - qw*pk)*k =                       */
/*                                                            */
/* (1) == (2)                                                 */
/*                                                            */
/**************************************************************/


#define BYTE(bit_idx) (*bit_idx >> 4)
#define BIT(bit_idx)  (*bit_idx & 0xF)
u16 math_bits_read_16(u16 *buf, u32 *buf_pos, u32 nbits)
{
  ASSERT(nbits <= 16);
  if (nbits == 0)
    return 0;

  u16 out = buf[BYTE(buf_pos)] >> BIT(buf_pos);

  u32 shift = BIT(buf_pos) > 16-nbits ? BIT(buf_pos) : 16-nbits;
  u16 mask = (((u16)~0) >> shift);

  out &= mask;

  if (BIT(buf_pos) + nbits < 16) {
    *buf_pos += nbits;
  }
  else {
    *buf_pos += nbits;

    mask = ((u16)~0) >> (16-BIT(buf_pos));
    u16 ext = buf[BYTE(buf_pos)] & mask;
    out |= ext << (nbits-BIT(buf_pos));
  }

  return out;
}

u16 math_bits_peek_16(u16 *buf, u32 buf_pos)
{
  u16 out = buf[BYTE(&buf_pos)] >> BIT(&buf_pos);

  out &= ((u16)~0) >> BIT(&buf_pos);

  buf_pos += 16;

  u16 mask = ((u16)~0) >> (16-BIT(&buf_pos));
  u16 ext = buf[BYTE(&buf_pos)] & mask;
  out |= ext << (16-BIT(&buf_pos));

  return out;
}


u32 math_endian_swap_u32(u32 n)
{
  n = (n >> 16) | (n << 16);
  return ((n & 0xFF00FF00) >> 8) | ((n & 0x00FF00FF) << 8);
}


u16 math_endian_swap_u16(u16 n)
{
  return (n >> 8) | (n << 8);
}

u16 math_mirror_all_u16(u16 n)
{
  n = (n & 0xAAAA) >> 1 | (n & 0x5555) << 1;
  n = (n & 0xCCCC) >> 2 | (n & 0x3333) << 2;
  n = (n & 0xF0F0) >> 4 | (n & 0x0F0F) << 4;
  n = (n & 0xFF00) >> 8 | (n & 0x00FF) << 8;
  return n;
}

u16 math_mirror_u16(u16 n, u16 n_bits)
{
  ASSERT(n_bits <= 16);
  n = (n & 0xAAAA) >> 1 | (n & 0x5555) << 1;
  n = (n & 0xCCCC) >> 2 | (n & 0x3333) << 2;
  n = (n & 0xF0F0) >> 4 | (n & 0x0F0F) << 4;
  n = (n & 0xFF00) >> 8 | (n & 0x00FF) << 8;
  n >>= 16-n_bits;
  return n;
}


u32 math_crc32(u8 *bytes, u32 len)
{
// #define POLYNOM 04 C1 | 1D B7
// #define POLYNOM 0000 0100 1100 0001 0001 1101 1011 0111
#define POLYNOM 0x04C11DB7

  u32 res = *(u32*)&bytes[len-4];
  u32 prev_bit_idx = len*8-1;
  s32 byte_idx = len-1;
  u8  bit_idx = 7;

  for (;;) {
    while (!bytes[byte_idx] && byte_idx >= 0) {
      --byte_idx;
      bit_idx = 7;
    }
    if (byte_idx == -1)
      return res;
    u8 byte = bytes[byte_idx];
    byte_idx = -1;
    for (u8 i = 0; i < 8; ++i) {
      if (byte & 0x80) {
        byte_idx = 7-i;
        break;
      }
      byte <<= 1;
    }
    ASSERT(byte_idx != -1);

    // xor polynom
    // shift res left and add bits from bytes from the right
    u32 shift = prev_bit_idx - byte_idx*8 + bit_idx;
    u32 r = byte_idx*8 + bit_idx - 32;
    if (shift < 32) {
      res <<= shift;
      
      res |= bytes[0];
    }
    else {
      res = 0;
    }


    prev_bit_idx = byte_idx*8 + bit_idx;
  }
}


u32 math_adler32(u8 *buf, u32 buf_len)
{
  u32 sum = 1;
  u32 sum_hist = 0;
  for (u32 i = 0; i < buf_len; ++i) {
    sum += (u8)*(buf++);
    sum %= 65521;

    sum_hist += sum;
    sum_hist %= 65521;
  }

  return (sum_hist<<16) | sum;
}


