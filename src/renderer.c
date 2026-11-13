

// Internal structs
typedef struct {
  r32 a; r32 b; r32 c;
} screen_line_t;

typedef struct {
  r32 y_min; r32 y_max;
  screen_line_t l;
  screen_line_t r;
} screen_triangle_t;


INTERNAL void render_background(game_offscreen_buffer_t *offscreen_buffer)
                               
{
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

INTERNAL void render_horizontal_line(game_offscreen_buffer_t *offscreen_buffer,
                                     game_depth_buffer_t     *depth_buffer,
                                     r32 r_y, r32 x_min, r32 x_max,
                                     vec2_t *screen_pos,
                                     r32    *distances,
                                     u32     color)
{
  s32 x0 = (s32)(x_min + 0.5);
  s32 x1 = (s32)(x_max + 0.5);
  if (x0 < 0) x0 = 0;
  if (x1 > offscreen_buffer->width) x1 = offscreen_buffer->width;

  s32 y = (s32)(r_y + 0.5);
  if (y < 0) y = 0;
  if (y >= offscreen_buffer->height) y = offscreen_buffer->height-1;

  u8 *row = (u8 *)offscreen_buffer->memory + y*offscreen_buffer->pitch;
  u8 *depth_row = (u8 *)depth_buffer->memory + y*depth_buffer->pitch;
  u32 *pixel = ((u32 *)row) + x0;
  r32 *depth = (r32 *)depth_row + x0;
  for (s32 x = x0; x < x1; ++x) {
    r32 l[3];
    math_barycentric((r32)x, (r32)y, screen_pos, l);

    r32 d = l[0]*distances[0] +
            l[1]*distances[1] +
            l[2]*distances[2];

    if (d > *depth) {
      *depth = d;
      // u32 r = (u32)(l[0] * 256);
      // u32 g = (u32)(l[1] * 256);
      // u32 b = (u32)(l[2] * 256);
      // 
      // *pixel = (r << 16) | (g << 8) | b;
      *pixel = color;
    }

    ++pixel;
    ++depth;
  }
}


INTERNAL void render_half_triangle(game_offscreen_buffer_t *offscreen_buffer,
                                   game_depth_buffer_t     *depth_buffer,
                                   screen_triangle_t       *screen_triangle,
                                   vec2_t                  *screen_pos,
                                   r32                     *distances,
                                   u32                      color)
{
  // clip min/max to screen
  s32 y_min = (s32)(screen_triangle->y_min + 0.5);
  s32 y_max = (s32)(screen_triangle->y_max + 0.5);
  if (y_min < 0) y_min = 0;
  if (y_max >= offscreen_buffer->height) y_max = offscreen_buffer->height-1;

  screen_line_t *left = &screen_triangle->l;
  screen_line_t *right = &screen_triangle->r;

  u8 *row = (u8 *)offscreen_buffer->memory + y_min*offscreen_buffer->pitch;
  u8 *depth_row = (u8 *)depth_buffer->memory + y_min*depth_buffer->pitch;
  for (s32 y = y_min; y <= y_max; ++y) {
    s32 x_min = (s32)(((left->c  - ((r32)y)*left->b)  / left->a)  + 0.5);
    s32 x_max = (s32)(((right->c - ((r32)y)*right->b) / right->a) + 0.5);
    if (x_min < 0) x_min = 0;
    if (x_max >= offscreen_buffer->width) x_max = offscreen_buffer->width-1;

    u32 *pixel = (u32 *)row + x_min;
    r32 *depth = (r32 *)depth_row + x_min;
    for (s32 x = x_min; x <= x_max; ++x) {

      r32 l[3];
      math_barycentric((r32)x, (r32)y, screen_pos, l);

      r32 d = l[0]*distances[0] +
              l[1]*distances[1] +
              l[2]*distances[2];

      if (d > *depth) {
        *depth = d;
        u32 r = (u32)(l[0] * 256);
        u32 g = (u32)(l[1] * 256);
        u32 b = (u32)(l[2] * 256);
        
        *pixel = (r << 16) | (g << 8) | b;
      }

      ++pixel;
      ++depth;
    }

    row += offscreen_buffer->pitch;
    depth_row += depth_buffer->pitch;
  }
}


INTERNAL void render_triangle(game_offscreen_buffer_t *offscreen_buffer,
                              game_depth_buffer_t     *depth_buffer,
                              triangle_t              *triangle,
                              u32                      color)
{
  s32 width  = offscreen_buffer->width;
  s32 height = offscreen_buffer->height;
  r32 f = 1/(r32)tanf(game_state->camera.fov/2);

  // move to camera
  vec3_t moved_p[3] = {
    math_sub_v3(triangle->p1, game_state->camera.position),
    math_sub_v3(triangle->p2, game_state->camera.position),
    math_sub_v3(triangle->p3, game_state->camera.position),
  };

  vec3_t triangle_norm = math_triangle_norm(triangle);
  // dont render triangles that doesn't face the camera
  for (u8 i = 0; i < 3; ++i) {
    r32 dot = math_dot_prod_3(moved_p[i], triangle_norm);
    if (dot < 0) {
      return;
    }
  }

  // rotate by camera rotation
  vec3_t cam_p[3] = {
    math_quaternion_rotate(game_state->camera.rotation, moved_p[0]),
    math_quaternion_rotate(game_state->camera.rotation, moved_p[1]),
    math_quaternion_rotate(game_state->camera.rotation, moved_p[2]),
  };

  r32 distances[3] = {
    1/math_norm_v3(cam_p[0]),
    1/math_norm_v3(cam_p[1]),
    1/math_norm_v3(cam_p[2]),
  };

  // dont render triangles that isn't within the angle of view
  u8 oob = TRUE;
  for (u8 i = 0; i < 3; ++i) {
    if (cam_p[i].z > fabs(cam_p[i].x) * f &&
        cam_p[i].z > fabs(cam_p[i].y) * f) {
      oob = FALSE;
      break;
    }
  }
  if (oob)
    return;

  // screen projection
  vec2_t orig_screen_pos[3];
  vec2_t screen_pos[4];

  for (u8 i = 0; i < 3; ++i) {
    // for now abort
    // future me: clip the triangle to only the part thats forward to the screen.
    // at least one point has to project further than the near plane
    if (cam_p[i].z < game_state->camera.near)
      return;

    //if (cam_p[i].z < game_state->camera.near)
    //  cam_p[i].z = game_state->camera.near;

    screen_pos[i].x = cam_p[i].x * f / cam_p[i].z;
    screen_pos[i].y = cam_p[i].y * f / cam_p[i].z;

    screen_pos[i].x = (screen_pos[i].x + 1.0F) * width/2.0F;
    screen_pos[i].y = (screen_pos[i].y + 1.0F) * height/2.0F;
  }

  for (u8 i = 0; i < 3; ++i)
    orig_screen_pos[i] = screen_pos[i];

#if 0
  for (u8 i = 0; i < 3; ++i)
    printf("screen_pos[%d](%f, %f)\n", i, screen_pos[i].x, screen_pos[i].y);
#endif

  // sort points from top to bottom (ascending in y)
  for (u8 i = 0; i < 3; ++i) {
    for (u8 j = i+1; j < 3; ++j) {
      if (screen_pos[i].y >= screen_pos[j].y) {
        vec2_t tmp = screen_pos[i];
        screen_pos[i] = screen_pos[j];
        screen_pos[j] = tmp;
        continue;
      }
    }
  }


  // slopes in the form ax + by = c
  // the presort ensures a(dy) > 0
  screen_line_t line[3];

  // describe triangle lines:
  // unwrap screen_pos[0] to screen_pos[3] to avoid modulo
  screen_pos[3] = screen_pos[0];
  for (u8  k = 0; k < 3; ++k) {
    r32 dy = screen_pos[k+1].y - screen_pos[k].y;
    r32 dx = screen_pos[k+1].x - screen_pos[k].x;
    // y-y0 = dy/dx * (x-x0)
    // dx*y = dy*x + dx*y0 - dy*x0
    // dy*x - dx*y = dy*x0 - dx*y0
    // a = dy, b = -dx, c = dy*x0 - dx*y0
    line[k].a =  dy;
    line[k].b = -dx;
    line[k].c =  dy*screen_pos[k].x - dx*screen_pos[k].y;
  }
  line[2].a *= -1;
  line[2].b *= -1;
  line[2].c *= -1;

  // for FULLY horizontal triangles: dy < 1 px, draw a line
  if (line[2].a < 1 || (line[1].a < 1 && line[0].a < 1)) {
    r32 x_min = screen_pos[0].x;
    r32 x_max = screen_pos[2].x;
    if (x_min > x_max) {
      x_min = screen_pos[2].x;
      x_max = screen_pos[0].x;
    }
    render_horizontal_line(offscreen_buffer,
                           depth_buffer,
                           screen_pos[0].y, x_min, x_max,
                           orig_screen_pos,
                           distances,
                           color);
    return;
  }

  // p3 := p1'
  screen_pos[3].y = screen_pos[1].y;
  screen_pos[3].x = (line[2].c - screen_pos[3].y*line[2].b) / line[2].a; // x = (c - y*b) / a, when a != 0

  screen_triangle_t screen_triangle;

  // top: p2, p1, p1'
  // skip if p1,p2 is horizontal
  if (line[1].a >= 1) {
    screen_triangle.y_max = screen_pos[2].y;
    screen_triangle.y_min = screen_pos[1].y;
    if (screen_pos[1].x < screen_pos[3].x) {
      screen_triangle.l = line[1];
      screen_triangle.r = line[2];
    } else {
      screen_triangle.l = line[2];
      screen_triangle.r = line[1];
    }
    render_half_triangle(offscreen_buffer, 
                         depth_buffer,
                         &screen_triangle,
                         orig_screen_pos,
                         distances,
                         color);
  }

  // bottom: p0, p1, p1'
  // skip if p0,p1 is horizontal
  if (line[0].a >= 1) {
    screen_triangle.y_max = screen_pos[1].y;
    screen_triangle.y_min = screen_pos[0].y;
    if (screen_pos[1].x < screen_pos[3].x) {
      screen_triangle.l = line[0];
      screen_triangle.r = line[2];
    } else {
      screen_triangle.l = line[2];
      screen_triangle.r = line[0];
    }
    render_half_triangle(offscreen_buffer, 
                         depth_buffer,
                         &screen_triangle,
                         orig_screen_pos,
                         distances,
                         color);
  }

  return;
}


