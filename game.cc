// #define DEBUG = 1;
#include "game.h"
#include "SDL.h"
#include "SDL_log.h"
#include "assert.h"
#include "cglm/cglm.h"

struct state {
  int32_t clear_color;
  int64_t start_ms;
  int64_t elapsed_ms;
  int64_t last_frame;
  int64_t elapsed_frames;

  vec3 camera_pos;
  vec3 camera_vel;

  mat4 viewport_tx;

  struct geometry {
    vec3 position;
    float z_rotation;
  } geometry;
} typedef state;

state g_state;

struct gfx_point {
  int x;
  int y;
} typedef gfx_point;

// clang-format off
float *cube_mesh =
    (float[]){-1.0, 1.0, -1.0, 
              1.0, -1.0, -1.0, 
              -1.0, -1.0, -1.0,
              -1.0, 1.0, -1.0, 
              1.0, 1.0, -1.0, 
              1.0, -1.0, -1.0,
              // right
              1.0, 1.0, -1.0,
              1.0, -1.0, 1.0,
              1.0, -1.0, -1.0,
              1.0, 1.0, -1.0,
              1.0, 1.0, 1.0,
              1.0, -1.0, 1.0,
              // left
              -1.0, 1.0, -1.0,
              -1.0, -1.0, 1.0,
              -1.0, -1.0, -1.0,
              -1.0, 1.0, -1.0,
              -1.0, 1.0, 1.0,
              -1.0, -1.0, 1.0,
              // back
              -1.0, 1.0, 1.0, 
              1.0, -1.0, 1.0,
              -1.0, -1.0, 1.0,
              -1.0, 1.0, 1.0, 
              1.0, 1.0, 1.0,
              1.0, -1.0, 1.0,
              // bottom
              -1.0, -1.0, -1.0,
              1.0, -1.0, 1.0,
              -1.0, -1.0, 1.0,
              -1.0, -1.0, -1.0,
              1.0, -1.0, -1.0,
              1.0, -1.0, 1.0,
              // top
              -1.0, 1.0, -1.0,
              1.0, 1.0, 1.0,
              -1.0, 1.0, 1.0,
              -1.0, 1.0, -1.0,
              1.0, 1.0, -1.0,
              1.0, 1.0, 1.0
              };
// clang-format on

inline void gfx_clear(SDL_Surface *surface, int32_t color) {
  int32_t *pixels = (int32_t *)surface->pixels;
  for (int i = 0; i < surface->w * surface->h; i++) {
    {
      pixels[i] = 0xFF222233;
    }
  }
}

inline void gfx_plot(SDL_Surface *surface, int x, int y, int32_t color) {
  if (x >= surface->w || y >= surface->h || x <= 0 || y <= 0) {
    return;
  }
  int32_t *pixels = (int32_t *)surface->pixels;
  pixels[x + y * surface->w] = color;
}

inline void gfx_draw_line(SDL_Surface *surface, vec3 from, vec3 to,
                          int32_t color) {
  // unoptimized [digital differential analyzer
  // (DDA)](https://en.wikipedia.org/wiki/Digital_differential_analyzer_(graphics_algorithm))
  float x, y;
  float dx = to[0] - from[0];
  float dy = to[1] - from[1];
  float step;
  if (fabs(dx) >= fabs(dy)) {
    step = fabs(dx);
  } else {
    step = fabs(dy);
  }
  if (step == 0.0) {
    SDL_LogDebug(0, "step == 0");
    return;
  }

  dx = dx / step;
  dy = dy / step;

  x = from[0];
  y = from[1];
  for (int i = 0; i < step; i++) {
    gfx_plot(surface, roundf(x), roundf(y), color);
    x = x + dx;
    y = y + dy;
  }
}

void gfx_draw_triangles(SDL_Surface *surface, float *vertices, int32_t count,
                        mat4 transform, int32_t color) {
  vec4 a;
  vec4 b;
  vec4 c;
  float *vp = vertices;
  for (int i = 0; i < count; i++) {
    memcpy(a, vp, sizeof(float) * 3);
    a[3] = 1.0;
    vp += 3;
    memcpy(b, vp, sizeof(float) * 3);
    b[3] = 1.0;
    vp += 3;
    memcpy(c, vp, sizeof(float) * 3);
    c[3] = 1.0;
    vp += 3;
    glm_mat4_mulv(transform, a, a);
    if (a[3] == 0.f) {
      return;
    }
    glm_vec4_divs(a, a[3], a);
    glm_mat4_mulv(transform, b, b);
    if (b[3] == 0.f) {
      return;
    }
    glm_vec4_divs(b, b[3], b);
    glm_mat4_mulv(transform, c, c);
    if (c[3] == 0.f) {
      return;
    }
    glm_vec4_divs(c, c[3], c);

    // TODO: need clipping here

    glm_mat4_mulv(g_state.viewport_tx, a, a);
    glm_mat4_mulv(g_state.viewport_tx, b, b);
    glm_mat4_mulv(g_state.viewport_tx, c, c);

    gfx_draw_line(surface, a, b, color);
    gfx_draw_line(surface, b, c, color);
    gfx_draw_line(surface, c, a, color);
  }
}

inline float wrapf(float v, float min, float max) {
  if (v > max) {
    return min + v - max;
  } else if (v < min) {
    return max - min - v;
  }
  return v;
}

void gm_vec2_clamp_and_scale(vec2 v, float min, float max) {
  float s = -INFINITY;
  if (v[0] > max) {
    s = fabs(1.f / v[0]);
  } else if (v[0] < min) {
    s = fabs(1.f / v[0]);
  }
  if (v[1] > max) {
    s = fabs(1.f / v[1]);
  } else if (v[0] < min) {
    s = fabs(1.f / v[1]);
  }
  if (s == -INFINITY) {
    return;
  }
  v[0] *= s;
  v[1] *= s;
}

// returns `false` if the line is completely outside clip space
inline bool gfx_clip_line(vec2 a, vec2 b, float min, float max) {
  // liang-barsky algorithm https://en.wikipedia.org/wiki/Liang–Barsky_algorithm
  float dx = b[0] - a[0];
  float dy = b[1] - a[1];

  float p[4] = {-dx, dx, -dy, dy};

  float q[4] = {a[0] - min, max - a[0], a[1] - min, max - a[1]};

  for (int i = 0; i < 4; i++) {
    if (p[i] == 0.f && q[i] < 0.f) {
      // line is parallel to this boundary and outside the orthogonal boundary
      return false;
    }
  }

  float u1 = -INFINITY;
  float u2 = INFINITY;

  for (int i = 0; i < 4; i++) {
    if (p[i] < 0.f) {
      u1 = fmaxf(u1, fmaxf(0.f, q[i] / p[i]));
    }
    if (p[i] > 0.f) {
      u2 = fminf(u2, fminf(1.f, q[i] / p[i]));
    }
  }
  fprintf(stderr, "clip_line: %.2f, %.2f\n", u1, u2);
  if (u1 > u2) {
    // outside
    return false;
  }
  if (u1 < 0.f && u2 > 1.f) {
    // completely inside
    return true;
  }
  a[0] = a[0] + dx * u1;
  a[1] = a[1] + dy * u1;
  b[0] = b[0] + dx * u2;
  b[1] = b[1] + dy * u2;

  return true;
}

void gfx_draw_lines_tx(SDL_Surface *surface, float *vertices, mat4 transform,
                       int count, int32_t color) {
  float *vp = vertices;
  vec4 a;
  vec4 b;
  for (int i = 0; i < count; i++) {
    glm_vec3_make(vp, a);
    a[3] = 1.f;
    vp += 3;
    glm_vec3_make(vp, b);
    b[3] = 1.f;
    vp += 3;
    glm_mat4_mulv(transform, a, a);
    if (a[3] == 0.f) {
      return;
    }
    glm_vec4_divs(a, a[3], a);
    glm_mat4_mulv(transform, b, b);
    if (b[3] == 0.f) {
      return;
    }
    glm_vec4_divs(b, b[3], b);

    // TODO: clip the line
    if (!gfx_clip_line(a, b, -1., 1.)) {
      return;
    }

    glm_mat4_mulv(g_state.viewport_tx, a, a);
    glm_mat4_mulv(g_state.viewport_tx, b, b);

    gfx_draw_line(surface, a, b, color);
  }
}

void gm_draw_axes(SDL_Surface *surface, mat4 transform, int32_t color) {
  float v2[] = {-10.f, 0.f, 0.f, 10.f, 0.f, 0.f};
  gfx_draw_lines_tx(surface, v2, transform, 1, 0xFFFF0000);

  float vertices[] = {0.f, 10.f, 0.f, 0.f, -10.f, 0.f};
  gfx_draw_lines_tx(surface, vertices, transform, 1, 0xFF00FF00);

  float v3[] = {0.f, 0.f, -10.f, 0.f, 0.f, 10.f};
  gfx_draw_lines_tx(surface, v3, transform, 1, 0xFF3333FF);
  float v4[] = {-1.f, 0.f, -10.f, -1.f, 0.f, 10.f};
  gfx_draw_lines_tx(surface, v4, transform, 1, 0xFF3333FF);
  float v5[] = {1.f, 0.f, -10.f, 1.f, 0.f, 10.f};
  gfx_draw_lines_tx(surface, v5, transform, 1, 0xFF3333FF);
}

int gm_start() {
  g_state.start_ms = SDL_GetTicks64();
  g_state.elapsed_ms = 0;
  g_state.last_frame = g_state.elapsed_ms;
  g_state.elapsed_frames = 0;
  glm_vec3_zero(g_state.camera_pos);
  g_state.camera_pos[2] = -2.0;
  glm_vec3_zero(g_state.geometry.position);
  g_state.geometry.position[1] = 0.5f;
  g_state.geometry.z_rotation = 0.f;
  // SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
  return 0;
}

int gm_process(SDL_Surface *surface) {

  int64_t current_ms = SDL_GetTicks64();
  int64_t current_elapsed_ms = current_ms - g_state.last_frame;
  float fps = 1000.f / current_elapsed_ms;
  float delta = current_elapsed_ms / 1000.f;
  SDL_LogDebug(0, "FPS: %.2f (%lld)", fps, current_elapsed_ms);
  g_state.elapsed_ms = current_ms;
  g_state.last_frame = current_ms;

  // clear
  gfx_clear(surface, 0xFF222233);

  vec3 translate;
  glm_vec3_zero(translate);
  translate[0] = delta * 20.f;

  mat4 transform, view, perspective;
  glm_mat4_identity(transform);
  glm_mat4_identity(g_state.viewport_tx);
  glm_mat4_identity(view);

  glm_perspective_default(float(surface->w) / float(surface->h), perspective);

  // move camera
  // TODO: change this to use camera basis vectors
  glm_vec3_add(g_state.camera_pos, g_state.camera_vel, g_state.camera_pos);
  glm_translate(view, g_state.camera_pos);

  glm_translate(g_state.viewport_tx,
                (vec3){surface->w / 2.f, surface->h / 2.f, 1.0});
  glm_scale(g_state.viewport_tx,
            (vec3){surface->w / 2.f, -surface->h / 2.f, 1.0});

  glm_mat4_identity(transform);
  glm_mat4_mul(view, transform, transform);
  glm_mat4_mul(perspective, transform, transform);
  gm_draw_axes(surface, transform, 0xFF999999);

  glm_mat4_identity(transform);
  //   g_state.geometry.position[0] += delta * 0.1;
  g_state.geometry.z_rotation += delta;

  glm_translate(transform, g_state.geometry.position);
  // glm_rotate_y(transform, sinf(g_state.geometry.z_rotation) * M_PI,
  // transform); glm_rotate_x(transform, sinf(g_state.geometry.z_rotation) *
  // M_PI, transform);
  glm_scale(transform, (vec3){0.5f, 0.5f, 0.5f});
  glm_mat4_mul(view, transform, transform);
  glm_mat4_mul(perspective, transform, transform);

  // gfx_draw_triangles(surface, cube_mesh, 12, transform, 0xFF00FFFF);

  vec3 test = {0.f, 0.f, 0.f};
  glm_mat4_mulv(transform, test, test);
  glm_vec3_print(test, stderr);
  g_state.elapsed_frames++;
  return 0;
}

int gm_quit() { return 0; }

void gm_keydown(const SDL_KeyboardEvent *evt) {
  float vel = 0.01;
  if (evt->keysym.scancode == SDL_SCANCODE_W) {
    g_state.camera_vel[2] = vel;
  } else if (evt->keysym.scancode == SDL_SCANCODE_S) {
    g_state.camera_vel[2] = -vel;
  } else if (evt->keysym.scancode == SDL_SCANCODE_Q) {
    g_state.camera_vel[1] = vel;
  } else if (evt->keysym.scancode == SDL_SCANCODE_E) {
    g_state.camera_vel[1] = -vel;
  } else if (evt->keysym.scancode == SDL_SCANCODE_A) {
    g_state.camera_vel[0] = vel;
  } else if (evt->keysym.scancode == SDL_SCANCODE_D) {
    g_state.camera_vel[0] = -vel;
  }
}

void gm_keyup(const SDL_KeyboardEvent *evt) {
  if (evt->keysym.scancode == SDL_SCANCODE_W) {
    g_state.camera_vel[2] = 0.0;
  } else if (evt->keysym.scancode == SDL_SCANCODE_S) {
    g_state.camera_vel[2] = 0.0;
  } else if (evt->keysym.scancode == SDL_SCANCODE_Q) {
    g_state.camera_vel[1] = 0.0;
  } else if (evt->keysym.scancode == SDL_SCANCODE_E) {
    g_state.camera_vel[1] = 0.0;
  } else if (evt->keysym.scancode == SDL_SCANCODE_A) {
    g_state.camera_vel[0] = 0.0;
  } else if (evt->keysym.scancode == SDL_SCANCODE_D) {
    g_state.camera_vel[0] = 0.0;
  }
}
