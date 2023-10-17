#define DEBUG = 1;
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
    gfx_draw_line(surface, a, b, color);
  }
}

void gm_draw_axes(SDL_Surface *surface, mat4 transform, int32_t color) {
  float vertices[] = {0.f, 10.f, 0.f, 0.f, -10.f, 0.f};
  gfx_draw_lines_tx(surface, vertices, transform, 1, 0xFF00FF00);
  float v2[] = {-10.f, 0.f, 0.f, 10.f, 0.f, 0.f};
  gfx_draw_lines_tx(surface, v2, transform, 1, 0xFFFF0000);
  float v3[] = {0.f, 0.f, -10.f, 0.f, 0.f, 10.f};
  gfx_draw_lines_tx(surface, v3, transform, 1, 0xFF3333FF);
}

int gm_start() {
  g_state.start_ms = SDL_GetTicks64();
  g_state.elapsed_ms = 0;
  g_state.last_frame = g_state.elapsed_ms;
  g_state.elapsed_frames = 0;
  glm_vec3_zero(g_state.camera_pos);
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

  mat4 transform, view, perspective, viewport;
  glm_mat4_identity(transform);
  glm_mat4_identity(viewport);
  glm_mat4_identity(view);

  glm_perspective_default(float(surface->w) / float(surface->h), perspective);

  // move camera
  glm_vec3_add(g_state.camera_pos, g_state.camera_vel, g_state.camera_pos);
  glm_translate(view, g_state.camera_pos);

  glm_translate(viewport, (vec3){surface->w / 2.f, surface->h / 2.f, 1.0});
  glm_scale(viewport, (vec3){surface->w / 2.f, -surface->h / 2.f, 1.0});

  glm_mat4_identity(transform);
  glm_mat4_mul(view, transform, transform);
  glm_mat4_mul(perspective, transform, transform);
  glm_mat4_mul(viewport, transform, transform);
  gm_draw_axes(surface, transform, 0xFF999999);

  glm_mat4_identity(transform);
  //   g_state.geometry.position[0] += delta * 0.1;
  g_state.geometry.z_rotation += delta;

  glm_translate(transform, g_state.geometry.position);
  glm_rotate_y(transform, sinf(g_state.geometry.z_rotation) * M_PI, transform);
  glm_scale(transform, (vec3){0.5f, 0.5f, 0.5f});
  glm_mat4_mul(view, transform, transform);
  glm_mat4_mul(perspective, transform, transform);
  glm_mat4_mul(viewport, transform, transform);

  gfx_draw_triangles(surface, cube_mesh, 12, transform, 0xFF00FFFF);

  g_state.elapsed_frames++;
  return 0;
}

int gm_quit() { return 0; }

void gm_keydown(const SDL_KeyboardEvent *evt) {
  // TODO: change this to use camera facing normal
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
