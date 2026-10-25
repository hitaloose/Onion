// Phase 1 smoke test for the Onion RG35XX rootfs.
// Draws red, green and blue bars (top to bottom) through SDL 1.2 fbcon and
// prints every SDL key event, so one run checks the video driver, the pixel
// order and the padmapd -> VT -> SDL keyboard path.
// The framebuffer keeps the panning left by the previous app, so it also
// prints the fb state and pans back to the first buffer, where SDL draws.
// Exits on MENU (SDLK_ESCAPE) or after 20 seconds.

#include <SDL/SDL.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void print_fb(const char *label, struct fb_var_screeninfo *vinfo)
{
    printf("fb %s: %dx%d virtual=%dx%d offset=%d,%d bpp=%d\n", label,
           vinfo->xres, vinfo->yres, vinfo->xres_virtual, vinfo->yres_virtual,
           vinfo->xoffset, vinfo->yoffset, vinfo->bits_per_pixel);
}

static void reset_fb_pan(void)
{
    struct fb_var_screeninfo vinfo;
    int fd = open("/dev/fb0", O_RDWR);

    if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("fb0");
        if (fd >= 0)
            close(fd);
        return;
    }

    print_fb("after SDL", &vinfo);
    if (vinfo.xoffset != 0 || vinfo.yoffset != 0) {
        vinfo.xoffset = 0;
        vinfo.yoffset = 0;
        if (ioctl(fd, FBIOPAN_DISPLAY, &vinfo) < 0)
            perror("FBIOPAN_DISPLAY");
        ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
        print_fb("after pan", &vinfo);
    }
    close(fd);
}

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
    reset_fb_pan();
    fflush(stdout);

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
