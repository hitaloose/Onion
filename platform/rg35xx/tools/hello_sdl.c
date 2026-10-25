// Phase 1 smoke test for the Onion RG35XX rootfs.
// Draws red, green and blue bars (top to bottom) through SDL 1.2 fbcon and
// prints every SDL key event, so one run checks the video driver, the pixel
// order and the padmapd -> VT -> SDL keyboard path.
// Exits on MENU (SDLK_ESCAPE) or after 20 seconds.

#include <SDL/SDL.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Surface *screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
    if (!screen) {
        fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    char driver[32];
    SDL_VideoDriverName(driver, sizeof(driver));
    const SDL_PixelFormat *fmt = screen->format;
    printf("driver=%s %dx%d bpp=%d Rshift=%d Gshift=%d Bshift=%d\n", driver,
           screen->w, screen->h, fmt->BitsPerPixel, fmt->Rshift, fmt->Gshift,
           fmt->Bshift);

    SDL_Rect bar = {0, 0, 640, 160};
    SDL_FillRect(screen, &bar, SDL_MapRGB(fmt, 255, 0, 0));
    bar.y = 160;
    SDL_FillRect(screen, &bar, SDL_MapRGB(fmt, 0, 255, 0));
    bar.y = 320;
    SDL_FillRect(screen, &bar, SDL_MapRGB(fmt, 0, 0, 255));
    SDL_Flip(screen);

    Uint32 deadline = SDL_GetTicks() + 20000;
    int quit = 0;
    SDL_Event event;

    while (!quit && SDL_GetTicks() < deadline) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                printf("%s %s (%d)\n", event.type == SDL_KEYDOWN ? "down" : "up",
                       SDL_GetKeyName(event.key.keysym.sym), event.key.keysym.sym);
                fflush(stdout);
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                    quit = 1;
            }
        }
        SDL_Delay(10);
    }

    SDL_Quit();
    return 0;
}
