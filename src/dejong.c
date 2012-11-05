#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "corange.h"

#include "dejong.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define NUM_THREADS 7
#define LOG_LUT_SIZE 32768 * NUM_THREADS

static short density[NUM_THREADS][SCREEN_HEIGHT][SCREEN_WIDTH];
static short max_density[NUM_THREADS];
static double log_lut[LOG_LUT_SIZE];

/* width of the "anti-aliasing" */
static double sample_width = 3.3;

/* depth of the mouse's control to the input vector */
static double sensitivity = 0.02;
static Uint16 mouseX;
static Uint16 mouseY;

/* global states for multithreading */
static Uint32 completion = 0;
static bool invalidated = 0;

static bool running = 0;
static bool frame_lock = 0;
static pthread_t main_thread;

double poisson_points[] = {
  0.797296f, 0.0357677f,
  0.764946f, 0.92288f,
  0.242775f, 0.884426f,
  0.119999f, 0.227851f,
  0.998596f, 0.558428f,
  0.447035f, 0.520157f,
  0.0036317f, 0.594317f
};

static char dejong_vars[64] = "a=.000000 b=.000000";

void dejong_init() {
  graphics_viewport_set_dimensions(SCREEN_WIDTH, SCREEN_HEIGHT);
  graphics_viewport_set_title("deJong");

  ui_button* vars_label = ui_elem_new("vars_label", ui_button);
  ui_button_move(vars_label, vec2_new(5,5));
  ui_button_resize(vars_label, vec2_new(190,25));
  ui_button_set_label(vars_label, dejong_vars);
  ui_button_disable(vars_label);

  for (int i = 0; i < LOG_LUT_SIZE; ++i) {
    log_lut[i] = log(i + 1);
  }

  pthread_create(&main_thread, NULL, dejong_density_thread, NULL);
}

typedef struct {
  double a;
  double b;
  double x;
  double y;
  int thread;
} density_args;

static void *dejong_density_thread(void *args) {
  pthread_t *density_threads = (pthread_t *)malloc(NUM_THREADS * sizeof(pthread_t));

  while(true) {
    double a, b;
    double x = SCREEN_WIDTH / 2.0;
    double y = SCREEN_HEIGHT / 2.0;
    a = ((double)mouseX * 2 / SCREEN_WIDTH - 1) * sensitivity;
    b = ((double)mouseY * 2 / SCREEN_WIDTH - 1) * sensitivity;
    sprintf(dejong_vars, "a=%0.6f, b=%0.6f", a, b);

    while (frame_lock) usleep(25);
    memset(density, 0, sizeof(short) * SCREEN_WIDTH * SCREEN_HEIGHT * NUM_THREADS);
    completion = 0;
    invalidated = 0;

    double sigma = (double)(sensitivity / (sample_width * SCREEN_WIDTH));
    for (int i = 0; i < NUM_THREADS; ++i) {
      max_density[i] = 1;
      density_args *thread_args = malloc(sizeof(density_args));
      /* Spin up threads with slightly different input vectors
       * This anti-aliases the output proportional to the number of threads.
       * (I'm not good at math, but) we can't derive x_n+2, y_n+2 from x_n/y_n;
       * therefore, this is the best way to utilise any extra CPU cores.
       */
      thread_args->a = a + sigma * poisson_points[2 * i] - sigma / 2;
      thread_args->b = b + sigma * poisson_points[2 * i + 1] - sigma / 2;
      thread_args->x = x;
      thread_args->y = y;
      thread_args->thread = i;
      pthread_create(&density_threads[i], NULL, dejong_density_loop, thread_args);
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
      pthread_join(density_threads[i], NULL);
    }
    while (!invalidated) usleep(125);
  }
  return NULL;
}

static void *dejong_density_loop(void *args) {
  density_args *my_args = (density_args *)args;
  /* May as well use the 80 bits we're given */
  long double a = my_args->a;
  long double b = my_args->b;
  long double x = my_args->x;
  long double y = my_args->y;
  int n = my_args->thread;
  unsigned short this_density = max_density[n];
  Uint32 start_time = SDL_GetTicks();
  while (true) {
    if ((completion = SDL_GetTicks() - start_time) > 33 && invalidated) break;
    if (max_density[n] >= 32767) {
      printf("[-----]Thread %d completed in %dms\n", n, completion);
      break;
    }
    /* A nice example of how SIN/COSPD would be a good thing */
    long double new_x, new_y;
    for (int i = 0; i < 8192; ++i) {
      new_x = (sin(a * y) - cos(b * x)) * 0.2 * SCREEN_WIDTH + SCREEN_WIDTH / 2.0;
      new_y = (sin(-a * x) - cos(-b * y)) * 0.2 * SCREEN_HEIGHT + SCREEN_HEIGHT / 2.0;
      x = new_x, y = new_y;
      this_density = (density[n][(int)y][(int)x] += 1);
      if (this_density > max_density[n]) {
        max_density[n] = this_density;
        if (this_density >= 32767) {
          break;
        }
      }
    }
  }
  free(args);
  return NULL;
}

void dejong_event(SDL_Event event) {
  
  switch(event.type) {
  case SDL_KEYDOWN:
    /* up/down increase/decrease the size of the poisson sample range */
    if (event.key.keysym.sym == SDLK_UP) { 
      sample_width = sample_width + 0.5;
      invalidated = 1;
    }
    if (event.key.keysym.sym == SDLK_DOWN) { 
      sample_width = max(sample_width-0.5, 0.3);
      invalidated = 1;
    }
    /* left/right increase/decrease the sensitivity of the mouse's position to the attractor parameters */
    if (event.key.keysym.sym == SDLK_LEFT) {
      sensitivity /= 1.03;
      invalidated = 1;
    }
    if (event.key.keysym.sym == SDLK_RIGHT) {
      sensitivity *= 1.03;
      invalidated = 1;
    }
    break;
  case SDL_MOUSEMOTION:
    mouseX = event.motion.x;
    mouseY = event.motion.y;
    invalidated = 1;
    break;
  }
    
}

void dejong_update() {
}

void dejong_render() {
  if (frame_lock) return;
  frame_lock = 1;
  
  glDisable(GL_DEPTH_TEST);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, graphics_viewport_width(), 0, graphics_viewport_height(), -1, 1);

  glBegin(GL_POINTS);
    int this_density, max_density_sum;
    double p_density, max_max_density = 0;
    for (int i = 0; i < SCREEN_WIDTH; ++i) {
      /* the position of this calculation is a compromise between performance and multithreading */
      max_density_sum = 0;
      for (int n = 0; n < NUM_THREADS; ++n) {
        max_density_sum += max_density[n];
      }
      if (!max_density_sum) continue;
      max_max_density = 1 / log(max_density_sum);

      for (int j = 0; j < SCREEN_HEIGHT; ++j) {
        this_density = 0;
        for (int n = 0; n < NUM_THREADS; ++n) {
          this_density += density[n][j][i];
        }
        if (!this_density) continue;
        /* Removing log from this loop is a huge performance gain for framerate.
         * One could resonably do this part on a GPU, with some amount of work.
         */
        p_density = log_lut[this_density - 1] * max_max_density;
        glColor3f(p_density, p_density, p_density);
        glVertex2f(i + 0.5, j + 0.5);
      }
    }

  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  SDL_GL_CheckError();
  frame_lock = 0;
}


int main(int argc, char **argv) {
  corange_init("../corange/core_assets");
  running = 1;

  dejong_init();
  
  while(running) {
    frame_begin();
    
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      
      switch(event.type){
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_ESCAPE) { running = 0; }
        if (event.key.keysym.sym == SDLK_PRINT) { graphics_viewport_screenshot(); }
        break;
      case SDL_QUIT:
        running = 0;
        break;
      break;
      }
      
      dejong_event(event);
      ui_event(event);
      
    }
    
    dejong_update();
    ui_update();
    
    dejong_render();
    ui_render();

    ui_button* vars_label = ui_elem_get("vars_label");
    ui_button_set_label(vars_label, dejong_vars);
    
    SDL_GL_SwapBuffers();
    
    frame_end_at_rate(30);
    
  }
  
  corange_finish();
  
  return 0;
}
