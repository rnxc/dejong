#ifndef dejong_h
#define dejong_h

void dejong_init();
void dejong_event(SDL_Event e);
void dejong_update();
void dejong_render();
void dejong_reset_density();
static void *dejong_density_loop(void *);
static void *dejong_density_thread(void *);

#endif
