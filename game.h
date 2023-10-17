#include <SDL.h>

int gm_start();
void gm_keydown(const SDL_KeyboardEvent *evt);
void gm_keyup(const SDL_KeyboardEvent *evt);
int gm_process(SDL_Surface *surface);
int gm_quit();
