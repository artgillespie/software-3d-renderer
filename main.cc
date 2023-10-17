#include "SDL_surface.h"
#include "SDL_video.h"
#include "game.h"
#include <SDL.h>

struct GLO_State {
  SDL_Window *window;
  SDL_Surface *surface;
  bool is_running;
} typedef GLO_State;

int main() {

  GLO_State state;
  memset(&state, 0, sizeof(GLO_State));

  int err = SDL_Init(SDL_INIT_VIDEO);
  if (err < 0) {
    SDL_Log("SDL failed to initialize: %d", err);
    return -1;
  }
  state.window = SDL_CreateWindow("VoxelEd", 0, 0, 960, 540, SDL_WINDOW_METAL);
  if (state.window == NULL) {
    SDL_Log("Couldn't create window");
    SDL_Quit();
  }

  state.surface = SDL_GetWindowSurface(state.window);
  if (state.surface == NULL) {
    SDL_Log("Couldn't create surface");
    SDL_Quit();
  }

  err = gm_start();
  if (err != 0) {
    SDL_LogError(0, "Error starting game %d", err);
    SDL_Quit();
  }

  const char *pfname = SDL_GetPixelFormatName(state.surface->format->format);
  SDL_LogInfo(0, "Pixel Format: %s", pfname);
  SDL_LogInfo(0, "bits per pixel: %d", state.surface->format->BitsPerPixel);

  state.is_running = true;

  SDL_Event event;
  while (state.is_running) {
    if (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        state.is_running = false;
        break;
      case SDL_MOUSEBUTTONDOWN:
        break;
      case SDL_MOUSEBUTTONUP:
        break;
      case SDL_MOUSEWHEEL:
        break;
      case SDL_KEYDOWN:
        gm_keydown(&event.key);
        break;
      case SDL_KEYUP:
        gm_keyup(&event.key);
        break;
      }
    }
    err = gm_process(state.surface);
    SDL_UpdateWindowSurface(state.window);
    SDL_Delay(1);
  }

  SDL_LogInfo(0, "Shutting down");
  SDL_FreeSurface(state.surface);
  SDL_DestroyWindow(state.window);
  return 0;
}
