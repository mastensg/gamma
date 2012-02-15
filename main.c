#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_rotozoom.h>
#include <SDL/SDL_ttf.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static const SDL_Color bg_color = {255, 255, 255, 255};
static const SDL_Color fg_color = {0, 0, 0, 255};

static SDL_Surface *image;
static SDL_Surface *screen;
static TTF_Font *thefont;
static int sw, sh;
static pthread_mutex_t mutex_update_image;

static void
draw_text(char *str, int x, int y, TTF_Font *font, SDL_Color color, int align) {
    SDL_Surface *text = TTF_RenderUTF8_Blended(font, str, color);

    SDL_Rect pos = {.x = x, .y = y};
    pos.x += align * text->w / 2;

    SDL_BlitSurface(text, NULL, screen, &pos);
    SDL_FreeSurface(text);
}

static void
draw(void) {
    int w = image->w;
    int h = image->h;

    SDL_Surface *src = image;
    if(w > sw || h > sh) {
        double zoomx = (double)sw / (double)w;
        double zoomy = (double)sh / (double)h;

        double zoom = zoomx;
        if(zoomy < zoom)
            zoom = zoomy;

        src = zoomSurface(image, zoom, zoom, 1);

        w = (double)w * zoom;
        h = (double)h * zoom;
    }

    SDL_Rect srcrect = {.x = 0, .y = 0, .w = w, .h = h};

    int x = 0;
    if(w < sw)
        x = (sw - w) / 2;

    int y = 0;
    if(h < sh)
        y = (sh - h) / 2;

    SDL_Rect dstrect = {.x = x, .y = y, .w = w, .h = h};

    SDL_FillRect(screen, NULL, 0xff999999);
    SDL_BlitSurface(src, &srcrect, screen, &dstrect);
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

    screen = SDL_SetVideoMode(sw, sh, 0, SDL_RESIZABLE);
    if(screen == NULL)
        err(EXIT_FAILURE, "SDL_SetVideoMode");
}

static void
handle_events(void) {
    SDL_Event event;
    SDL_WaitEvent(&event);

    switch(event.type) {
    case SDL_KEYDOWN:
        if(event.key.keysym.sym == SDLK_ESCAPE)
            exit(EXIT_SUCCESS);
        break;

    case SDL_VIDEORESIZE:
        sw = event.resize.w;
        sh = event.resize.h;
        screen = SDL_SetVideoMode(sw, sh, 0, SDL_RESIZABLE);
        break;

    case SDL_QUIT:
        exit(EXIT_SUCCESS);
        break;
    }
}

static void
update_image(char *path) {
    pthread_mutex_lock(&mutex_update_image);
    image = IMG_Load(path);
    pthread_mutex_unlock(&mutex_update_image);
}

static void *
monitor_file(void *arg) {
    char *path = arg;

    int fd = inotify_init1(IN_NONBLOCK);
    if(fd == -1)
        err(EXIT_FAILURE, "inotify_init1");

    if(inotify_add_watch(fd, path, IN_CLOSE_WRITE) == -1)
        err(EXIT_FAILURE, "inotify_add_watch");

    for(;;) {
        fd_set set;
        FD_SET(fd, &set);
        if(select(fd + 1, &set, NULL, NULL, NULL) == -1)
            err(EXIT_FAILURE, "select");

        if(!(FD_ISSET(fd, &set)))
            continue;

        int numbytes;
        if(ioctl(fd, FIONREAD, &numbytes) == -1)
            err(EXIT_FAILURE, "ioctl");

        char *buf[65536];
        if(read(fd, buf, sizeof(buf)) == -1)
            err(EXIT_FAILURE, "read");

        update_image(path);
    }

    return NULL;
}

int
main(int argc, char **argv) {
    if(argc != 2)
        errx(EXIT_FAILURE, "no input files");

    pthread_mutex_init(&mutex_update_image, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_t inotify_thread;
    if(pthread_create(&inotify_thread, &attr, monitor_file, argv[1]))
        err(EXIT_FAILURE, "pthread_create");

    update_image(argv[1]);
    font_init();
    screen_init();

    for(;;) {
        handle_events();
        draw();
    }

    SDL_Quit();

    return EXIT_SUCCESS;
}
