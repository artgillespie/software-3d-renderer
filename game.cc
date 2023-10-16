// #define DEBUG = 1;
#include "game.h"
#include "SDL.h"
#include "SDL_log.h"
#include "cglm/cglm.h"

struct state {
  int32_t clear_color;
  int64_t start_ms;
  int64_t elapsed_ms;
  int64_t last_frame;
  int64_t elapsed_frames;

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
  vec3 a;
  vec3 b;
  vec3 c;
  float *vp = vertices;
  for (int i = 0; i < count; i++) {
    glm_vec3_make(vp, a);
    vp += 3;
    glm_vec3_make(vp, b);
    vp += 3;
    glm_vec3_make(vp, c);
    vp += 3;
    glm_mat4_mulv3(transform, a, 1.f, a);
    glm_mat4_mulv3(transform, b, 1.f, b);
    glm_mat4_mulv3(transform, c, 1.f, c);

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

int gm_start() {
  g_state.start_ms = SDL_GetTicks64();
  g_state.elapsed_ms = 0;
  g_state.last_frame = g_state.elapsed_ms;
  g_state.elapsed_frames = 0;
  glm_vec3_zero(g_state.geometry.position);
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

  mat4 transform, viewport, perspective;
  glm_mat4_identity(transform);
  glm_mat4_identity(viewport);
  glm_perspective(M_PI_2, float(surface->w) / float(surface->h), 0.1, 100.0,
                  perspective);
  g_state.geometry.position[0] += delta * 0.1;
  if (g_state.geometry.position[0] >= 1.0) {
    g_state.geometry.position[0] -= 2.0;
  }
  g_state.geometry.z_rotation += delta;

  // glm_translate(transform, g_state.geometry.position);
  glm_rotate_z(transform, g_state.geometry.z_rotation, transform);

  glm_translate(viewport, (vec3){surface->w / 2.f, surface->h / 2.f, 1.0});
  glm_scale(viewport, (vec3){surface->w / 2.f, -surface->h / 2.f, 1.0});
  glm_mat4_mul(viewport, transform, transform);
  glm_mat4_mul(perspective, transform, transform);

  float geometry[] = {-0.5, -0.5, 0.0, 0.5, -0.5, 0.0, -0.5, 0.5,  0.0,
                      -0.5, 0.5,  0.0, 0.5, 0.5,  0.0, 0.5,  -0.5, 0.0};
  gfx_draw_triangles(surface, geometry, 2, transform, 0xFF00FF00);

  g_state.elapsed_frames++;
  return 0;
}

int gm_quit() { return 0; }
