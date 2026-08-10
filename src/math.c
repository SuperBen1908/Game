

#define MATH_IMPLE(N, vec_t, mat_t)                                                  \
INTERNAL inline vec_t math_add_v##N(vec_t v1, vec_t v2)                              \
{                                                                                    \
  vec_t v = { 0 }; for (u8 i = 0; i < N; ++i) v.v[i] = v1.v[i] + v2.v[i]; return v;  \
}                                                                                    \
                                                                                     \
INTERNAL inline vec_t math_sub_v##N(vec_t v1, vec_t v2)                              \
{                                                                                    \
  vec_t v = { 0 }; for (u8 i = 0; i < N; ++i) v.v[i] = v1.v[i] - v2.v[i]; return v;  \
}                                                                                    \
                                                                                     \
INTERNAL inline vec_t math_mul_c_v##N(real32 c, vec_t v)                             \
{                                                                                    \
  for (u8 i = 0; i < N; ++i) v.v[i] *= c; return v;                                  \
}                                                                                    \
                                                                                     \
INTERNAL inline real32 math_dot_prod_##N(vec_t v1, vec_t v2)                         \
{                                                                                    \
  real32 s =  0 ; for (u8 i = 0; i < N; ++i) s += v1.v[i]*v2.v[i]; return s;         \
}                                                                                    \
                                                                                     \
INTERNAL inline real32 math_norm_v##N(vec_t v)                                       \
{                                                                                    \
  return sqrtf(math_dot_prod_##N(v, v));                                             \
}                                                                                    \
                                                                                     \
INTERNAL inline mat_t math_mul_mat_##N(mat_t m1, mat_t m2)                           \
{                                                                                    \
  mat_t m = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) for (u8 j = 0; j < N; ++j) {                            \
    m.m[i][j] =  0 ; for (u8 k = 0; k < N; ++k) m.m[i][j] += m1.m[i][k]*m2.m[k][j];  \
  }                                                                                  \
  return m;                                                                          \
}                                                                                    \
                                                                                     \
INTERNAL inline vec_t math_mul_mat_vec_##N(mat_t m, vec_t v1)                        \
{                                                                                    \
  vec_t v = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) {                                                       \
    v.v[i] =  0 ; for (u8 k = 0; k < N; ++k) v.v[i] += m.m[i][k]*v1.v[k];            \
  }                                                                                  \
  return v;                                                                          \
}                                                                                    \
                                                                                     \
INTERNAL inline vec_t math_mul_vec_mat_##N(vec_t v1, mat_t m)                        \
{                                                                                    \
  vec_t v = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) {                                                       \
    v.v[i] =  0 ; for (u8 k = 0; k < N; ++k) v.v[i] += v1.v[k]*m.m[k][i];            \
  }                                                                                  \
  return v;                                                                          \
}                                                                                    \
                                                                                     \
INTERNAL inline mat_t math_trans_mat_##N(mat_t m1)                                   \
{                                                                                    \
  mat_t m = { 0 };                                                                   \
  for (u8 i = 0; i < N; ++i) for (u8 j = 0; j < N; ++j) m.m[i][j] = m1.m[j][i];      \
  return m;                                                                          \
}
MATH_IMPLE(2, vec2_t, mat2_t) MATH_IMPLE(3, vec3_t, mat3_t) MATH_IMPLE(4, vec4_t, mat4_t)


// corss product
INTERNAL inline vec3_t math_cross_prod(vec3_t v1, vec3_t v2)
{
  return (vec3_t) {
    .x = v1.y*v2.z - v1.z*v2.y,
    .y = v1.z*v2.x - v1.x*v2.z,
    .z = v1.x*v2.y - v1.y*v2.x,
  };
}


// triangle
INTERNAL vec3_t math_triangle_norm(triangle_t *triangle)
{
  return math_cross_prod(math_sub_v3(triangle->p2, triangle->p1),
                         math_sub_v3(triangle->p3, triangle->p1));
}


INTERNAL vec3_t math_triangle_center(triangle_t *triangle)
{
  return (vec3_t) {
    .x = (triangle->p1.x + triangle->p2.x + triangle->p3.x) / 3,
    .y = (triangle->p1.y + triangle->p2.y + triangle->p3.y) / 3,
    .z = (triangle->p1.z + triangle->p2.z + triangle->p3.z) / 3,
  };
}


// euler angles
INTERNAL mat3_t math_roll_mat(real32 roll)
{
  real32 s = sinf(roll);
  real32 c = cosf(roll);
  return (mat3_t) { .m = {
    { 1,  0, 0 },
    { 0,  c, s },
    { 0, -s, c },
  }};
}


INTERNAL mat3_t math_pitch_mat(real32 pitch)
{
  real32 s = sinf(pitch);
  real32 c = cosf(pitch);
  return (mat3_t) { .m = {
    {  c, 0, s },
    {  0, 1, 0 },
    { -s, 0, c },
  }};
}


INTERNAL mat3_t math_yaw_mat(real32 yaw)
{
  real32 s = sinf(yaw);
  real32 c = cosf(yaw);
  return (mat3_t) { .m = {
    {  c, s, 0 },
    { -s, c, 0 },
    {  0, 0, 1 },
  }};
}


// quaternions
INTERNAL quaternion_t math_quaternion_mul(quaternion_t q1, quaternion_t q2)
{
  quaternion_t q = { 0 };
  q.w = q1.w*q2.w - q1.i*q2.i - q1.j*q2.j - q1.k*q2.k;
  q.i = q1.w*q2.i + q1.i*q2.w + q1.j*q2.k - q1.k*q2.j;
  q.j = q1.w*q2.j - q1.i*q2.k + q1.j*q2.w + q1.k*q2.i;
  q.k = q1.w*q2.k + q1.i*q2.j - q1.j*q2.i + q1.k*q2.w;
  return q;
}


INTERNAL quaternion_t math_quaternion(vec3_t axis, real32 angle)
{
  quaternion_t q = { 0 };
  real32 c = cosf(angle/2);
  real32 s = sinf(angle/2);
  q.v = math_mul_c_v3(s, axis);
  q.w = c;
  return q;
}


INTERNAL vec3_t math_quaternion_rotate(quaternion_t q, vec3_t v)
{
  quaternion_t p = {.v = v, .w = 0};
  p = math_quaternion_mul(q, p);
  q.i *= -1.0F; q.j *= -1.0F; q.k *= -1.0F;
  p = math_quaternion_mul(p, q);
  return p.v;
}


INTERNAL vec3_t math_quaternion_rotate_inv(quaternion_t q, vec3_t v)
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






