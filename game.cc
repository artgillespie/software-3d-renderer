#include "game.h"
#include "SDL.h"
#include "SDL_log.h"

struct state {
  int32_t clear_color;
  int64_t start_ms;
  int64_t elapsed_ms;
  int64_t last_frame;
  int64_t elapsed_frames;

  struct geometry {
    float x_pos;
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
  int32_t *pixels = (int32_t *)surface->pixels;
  pixels[x + y * surface->w] = color;
}

inline void gfx_draw_line(SDL_Surface *surface, gfx_point from, gfx_point to,
                          int32_t color) {
  // unoptimized [digital differential analyzer
  // (DDA)](https://en.wikipedia.org/wiki/Digital_differential_analyzer_(graphics_algorithm))
  float x, y;
  float dx = to.x - from.x;
  float dy = to.y - from.y;
  float step;
  if (fabs(dx) >= fabs(dy)) {
    step = fabs(dx);
  } else {
    step = fabs(dy);
  }

  dx = dx / step;
  dy = dy / step;

  x = from.x;
  y = from.y;
  for (int i = 0; i < step; i++) {
    gfx_plot(surface, roundf(x), roundf(y), color);
    x = x + dx;
    y = y + dy;
  }
}

void gfx_draw_triangle(SDL_Surface *surface, gfx_point A, gfx_point B,
                       gfx_point C, int32_t color) {
  gfx_draw_line(surface, A, B, color);
  gfx_draw_line(surface, B, C, color);
  gfx_draw_line(surface, C, A, color);
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
  g_state.geometry.x_pos = 0.f;
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

  g_state.geometry.x_pos =
      wrapf(g_state.geometry.x_pos + delta * 20.f, 0.f, surface->w);

  gfx_point triangle[3];
  triangle[0].x = g_state.geometry.x_pos;
  triangle[0].y = 100;
  triangle[1].x = g_state.geometry.x_pos + 50;
  triangle[1].y = 200;
  triangle[2].x = g_state.geometry.x_pos + 100;
  triangle[2].y = 100;
  gfx_draw_triangle(surface, triangle[0], triangle[1], triangle[2], 0xFF00FF00);

  g_state.elapsed_frames++;
  return 0;
}

int gm_quit() { return 0; }
