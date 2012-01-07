#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static const SDL_Color bg_color = {255, 255, 255, 255};
static const SDL_Color fg_color = {0, 0, 0, 255};

static SDL_Surface *screen;
static TTF_Font *thefont;
static int sw, sh;

static void
draw_text(char *str, int x, int y, TTF_Font *font, SDL_Color color, int rightalign) {
    SDL_Surface *text = TTF_RenderUTF8_Shaded(font, str, color, bg_color);

    SDL_Rect pos = {x, y};
    if(rightalign)
        pos.x -= text->w;

    SDL_BlitSurface(text, NULL, screen, &pos);
    SDL_FreeSurface(text);
}

static void
draw(void) {
    SDL_FillRect(screen, &screen->clip_rect, 0xffffff);

    draw_text("hei!", 10, 10, thefont, fg_color, 0);

    SDL_Flip(screen);
}

static void
font_init(void) {
    if(TTF_Init() == -1)
        err(EXIT_FAILURE, "cannot initialize font library");

    char *fontpath = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf";
    thefont = TTF_OpenFont(fontpath, 16);
    if(thefont == NULL)
        err(EXIT_FAILURE, "cannot load font \"%s\"", fontpath);
}

static void
screen_init() {
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

    const SDL_VideoInfo *info = SDL_GetVideoInfo();
    if(info == NULL)
        err(EXIT_FAILURE, "SDL_GetVideoInfo");

    sw = info->current_w;
    sh = info->current_h;

    screen = SDL_SetVideoMode(sh / 2, sh / 2, 0, SDL_RESIZABLE);
    if(screen == NULL)
        err(EXIT_FAILURE, "cannot initialize screen");
}

static void
handle_events(void) {
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_ESCAPE)
                exit(EXIT_SUCCESS);
            break;
        case SDL_VIDEORESIZE:
            sw = event.resize.w;
            sh = event.resize.h;
            break;
        case SDL_QUIT:
            exit(EXIT_SUCCESS);
            break;
        }
    }
}

int
main(int argc, char **argv) {
    font_init();
    screen_init();

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_t inotify_thread;
    if(pthread_create(&inotify_thread, &attr, networking, NULL))
        err(EXIT_FAILURE, "pthread_create");

    for(;;) {
        handle_events();
        draw();
    }

    SDL_Quit();

    return EXIT_SUCCESS;
}
