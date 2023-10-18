// #define DEBUG = 1;
#include "game.h"
#include "SDL.h"
#include "SDL_log.h"
#include "assert.h"
#include "cglm/cglm.h"
#include <sstream>
#include <string>

struct vertex {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
} typedef vertex;

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
    std::vector<vertex> mesh;
  } geometry;
} typedef state;

state g_state;

struct gfx_point {
  int x;
  int y;
} typedef gfx_point;

// load vertices from an .obj file
// **this is not robust** — it only looks at vertex and face lines
// and expects all faces to be triangulated without verifying that
// they are
int io_load_mesh_from_obj(const char *filename, std::vector<vertex> &mesh) {
  std::vector<vertex> vertices;

  size_t size;
  void *data = SDL_LoadFile(filename, &size);
  if (data == NULL) {
    return -1;
  }
  std::string dstr(static_cast<char *>(data));
  std::stringstream ss(dstr);

  while (!ss.eof()) {
    char line[512];
    ss.getline(line, 512);
    if (strlen(line) == 0) {
      continue;
    }
    std::stringstream ssl(line);
    char cmd = line[0];
    switch (cmd) {
    case 'v': {
      vertex v;
      ssl >> cmd >> v.x >> v.y >> v.z;
      vertices.push_back(v);
      break;
    }
    case 'f': {
      vertex a;
      vertex b;
      vertex c;
      int i, j, k;
      ssl >> cmd >> i >> j >> k;
      mesh.push_back(vertices[i - 1]);
      mesh.push_back(vertices[j - 1]);
      mesh.push_back(vertices[k - 1]);
      break;
    }
    }
  }

  SDL_free(data);
  return 0;
}

inline void gfx_clear(SDL_Surface *surface, int32_t color) {
  int32_t *pixels = (int32_t *)surface->pixels;
  for (int i = 0; i < surface->w * surface->h; i++) {
    {
      pixels[i] = 0xFF222233;
    }
  }
}

// in screen coordinate space
inline void gfx_plot(SDL_Surface *surface, int x, int y, int32_t color) {
  if (x >= surface->w || y >= surface->h || x <= 0 || y <= 0) {
    return;
  }
  int32_t *pixels = (int32_t *)surface->pixels;
  pixels[x + y * surface->w] = color;
}

// in screen coordinate space
inline void gfx_draw_line(SDL_Surface *surface, vec2 from, vec2 to,
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

// in screen coordinate space
inline void gfx_fill_triangle(SDL_Surface *surface, const vec2 &a,
                              const vec2 &b, const vec2 &c, int32_t color) {
  // hacked-together algorithm from first principles, **not optimized at all**
  // sort the vertices by their y-component
  // longest side is p0 -> p2
  // for both p0.y -> p1.y and p1.y -> p2.y
  //    interpolate f(y) -> x1 for the long side p0 -> p2
  //    interpolate f(y) -> x2 for the current side (either p0->p1 or p1->p2)
  //    draw_line(x1, y, x2, y)
  vec2 p[3];
  memcpy(p[0], a, sizeof(vec2));
  memcpy(p[1], b, sizeof(vec2));
  memcpy(p[2], c, sizeof(vec2));

  // sort lowest-to-highest y
  if (p[1][1] < p[0][1]) {
    vec2 t;
    memcpy(t, p[0], sizeof(vec2));
    memcpy(p[0], p[1], sizeof(vec2));
    memcpy(p[1], t, sizeof(vec2));
  }
  if (p[2][1] < p[0][1]) {
    vec2 t;
    memcpy(t, p[0], sizeof(vec2));
    memcpy(p[0], p[2], sizeof(vec2));
    memcpy(p[2], t, sizeof(vec2));
  }
  if (p[2][1] < p[1][1]) {
    vec2 t;
    memcpy(t, p[1], sizeof(vec2));
    memcpy(p[1], p[2], sizeof(vec2));
    memcpy(p[2], t, sizeof(vec2));
  }

  float ac_dy = 1.f / (p[2][1] - p[0][1]);
  float ab_dy = 1.f / (p[1][1] - p[0][1]);
  float bc_dy = 1.f / (p[2][1] - p[1][1]);

  // fill "top"
  for (int y = p[0][1]; y < p[1][1]; y++) {
    vec2 a;
    vec2 b;
    a[0] = (y - p[0][1]) * ac_dy * (p[2][0] - p[0][0]) + p[0][0];
    a[1] = y;
    b[0] = (y - p[0][1]) * ab_dy * (p[1][0] - p[0][0]) + p[0][0];
    b[1] = y;
    if (b[0] > a[0]) {
      for (int x = a[0]; x < b[0]; x++) {
        gfx_plot(surface, x, y, color);
      }
    } else {
      for (int x = b[0]; x < a[0]; x++) {
        gfx_plot(surface, x, y, color);
      }
    }
  }
  // fill "bottom"
  for (int y = p[1][1]; y < p[2][1]; y++) {
    vec2 a;
    vec2 b;
    a[0] = (y - p[0][1]) * ac_dy * (p[2][0] - p[0][0]) + p[0][0];
    a[1] = y;
    b[0] = (y - p[1][1]) * bc_dy * (p[2][0] - p[1][0]) + p[1][0];
    b[1] = y;
    if (b[0] > a[0]) {
      for (int x = a[0]; x < b[0]; x++) {
        gfx_plot(surface, x, y, color);
      }
    } else {
      for (int x = b[0]; x < a[0]; x++) {
        gfx_plot(surface, x, y, color);
      }
    }
  }
}

// TODO: need to pass transforms in individually since we need
// to clip before the projection transform
void gfx_draw_triangles(SDL_Surface *surface, std::vector<vertex> &mesh,
                        mat4 transform, int32_t color) {
  vec4 a;
  vec4 b;
  vec4 c;
  for (int i = 0; i < mesh.size();) {
    memcpy(a, &mesh[i], sizeof(float) * 3);
    a[3] = 1.0;
    i++;
    memcpy(b, &mesh[i], sizeof(float) * 3);
    b[3] = 1.0;
    i++;
    memcpy(c, &mesh[i], sizeof(float) * 3);
    c[3] = 1.0;
    i++;
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

void gfx_fill_triangles(SDL_Surface *surface, std::vector<vertex> &mesh,
                        mat4 transform, int32_t color) {}

inline float wrapf(float v, float min, float max) {
  if (v > max) {
    return min + v - max;
  } else if (v < min) {
    return max - min - v;
  }
  return v;
}

// returns `false` if the line is completely outside clip space
inline bool gfx_clip_line(vec3 a, vec3 b, float min, float max) {
  // liang-barsky algorithm https://en.wikipedia.org/wiki/Liang–Barsky_algorithm
  float dx = b[0] - a[0];
  float dy = b[1] - a[1];
  float dz = b[2] - a[2];

  float p[6] = {-dx, dx, -dy, dy, -dz, dz};

  float q[6] = {a[0] - min, max - a[0], a[1] - min,
                max - a[1], a[2] - min, max - a[2]};

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

// vertices in world-space
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
    if (!gfx_clip_line(a, b, -1., 1.)) {
      return;
    }

    glm_vec4_divs(b, b[3], b);

    glm_mat4_mulv(g_state.viewport_tx, a, a);
    glm_mat4_mulv(g_state.viewport_tx, b, b);

    gfx_draw_line(surface, a, b, color);
  }
}

void gm_draw_axes(SDL_Surface *surface, mat4 transform, int32_t color) {
  /*
  float v2[] = {-10.f, 0.f, 0.f, 10.f, 0.f, 0.f};
  gfx_draw_lines_tx(surface, v2, transform, 1, 0xFFFF0000);

  float vertices[] = {0.f, 10.f, 0.f, 0.f, -10.f, 0.f};
  gfx_draw_lines_tx(surface, vertices, transform, 1, 0xFF00FF00);
  */

  float v3[] = {0.f, 0.f, -10.f, 0.f, 0.f, 10.f};
  gfx_draw_lines_tx(surface, v3, transform, 1, 0xFF3333FF);
  // float v4[] = {-1.f, 0.f, -10.f, -1.f, 0.f, 10.f};
  // gfx_draw_lines_tx(surface, v4, transform, 1, 0xFF3333FF);
  // float v5[] = {1.f, 0.f, -10.f, 1.f, 0.f, 10.f};
  // gfx_draw_lines_tx(surface, v5, transform, 1, 0xFF3333FF);
}

int gm_start() {
  io_load_mesh_from_obj("../cube.obj", g_state.geometry.mesh);
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

void gm_debug_tx_pt(mat4 view, mat4 perspective, vec4 a, vec4 b) {
  fprintf(stderr, "debug point translation\n");
  fprintf(stderr, "A: ");
  glm_vec4_print(a, stderr);
  fprintf(stderr, "B: ");
  glm_vec4_print(b, stderr);

  fprintf(stderr, "camera pos: ");
  glm_vec3_print(g_state.camera_pos, stderr);
  glm_mat4_print(view, stderr);

  glm_mat4_mulv(view, a, a);
  glm_mat4_mulv(view, b, b);

  fprintf(stderr, "-- post view translation\n");
  fprintf(stderr, "A: ");
  glm_vec4_print(a, stderr);
  fprintf(stderr, "B: ");
  glm_vec4_print(b, stderr);

  glm_mat4_mulv(perspective, a, a);
  glm_mat4_mulv(perspective, b, b);

  fprintf(stderr, "-- post perspective translation\n");
  fprintf(stderr, "A: ");
  glm_vec4_print(a, stderr);
  fprintf(stderr, "B: ");
  glm_vec4_print(b, stderr);

  if (a[3] == 0.f) {
    fprintf(stderr, "divide by w zero: pt A");
    return;
  }
  if (b[3] == 0.f) {
    fprintf(stderr, "divide by w zero: pt B");
    return;
  }
  glm_vec4_divs(a, a[3], a);
  glm_vec4_divs(b, b[3], b);
  fprintf(stderr, "-- post perspective divide\n");
  fprintf(stderr, "A: ");
  glm_vec4_print(a, stderr);
  fprintf(stderr, "B: ");
  glm_vec4_print(b, stderr);
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

  vec2 a = {200, 200};
  vec2 b = {250, 380};
  vec2 c = {100, 275};

  gfx_fill_triangle(surface, a, b, c, 0xFFFFFFFF);

  gfx_draw_line(surface, a, b, 0xFFFF0000);
  gfx_draw_line(surface, b, c, 0xFFFF0000);
  gfx_draw_line(surface, c, a, 0xFFFF0000);

  return 0;

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
  //   g_state.geometry.position[0] += delta * 0.1;
  g_state.geometry.z_rotation += delta;

  glm_translate(transform, g_state.geometry.position);
  // glm_rotate_y(transform, sinf(g_state.geometry.z_rotation) * M_PI,
  // transform); glm_rotate_x(transform, sinf(g_state.geometry.z_rotation) *
  // M_PI, transform);
  glm_scale(transform, (vec3){0.5f, 0.5f, 0.5f});
  glm_mat4_mul(view, transform, transform);
  glm_mat4_mul(perspective, transform, transform);

  gfx_draw_triangles(surface, g_state.geometry.mesh, transform, 0xFF00FFFF);

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
