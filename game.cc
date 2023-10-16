#include "game.h"
#include "SDL.h"

struct state {
  int32_t clear_color;

} typedef state;

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

void gfx_draw_line(SDL_Surface *surface, int from_x, int from_y, int to_x,
                   int to_y, int32_t color) {
  int dx = to_x - from_x;
  int dy = to_y - from_y;
  float D = 2 * dy - dx;
  int y = from_y;

  for (int x = from_x; x < to_x; x++) {
    gfx_plot(surface, x, y, color);
    if (D > 0) {
      y++;
      D -= 2 * dx;
    }
    D += 2 * dy;
  }
}

int gm_start() { return 0; }

int gm_process(SDL_Surface *surface) {

  // clear
  gfx_clear(surface, 0xFF222233);
  gfx_draw_line(surface, 0, 0, surface->w, surface->h, 0xFFFF0000);
  gfx_draw_line(surface, 0, surface->h, surface->w, 0, 0xFF00FF00);
  return 0;
}

int gm_quit() { return 0; }
