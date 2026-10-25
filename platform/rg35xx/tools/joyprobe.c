// Prints what SDL 1.2 sees on joystick 0 (axes, hats, buttons) for a few
// seconds, to check RetroArch autoconfig mappings. Usage: joyprobe [seconds]
#include <SDL/SDL.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int seconds = argc > 1 ? atoi(argv[1]) : 10;
    SDL_Init(SDL_INIT_JOYSTICK);
    SDL_Joystick *joy = SDL_JoystickOpen(0);
    if (!joy) {
        printf("no joystick: %s\n", SDL_GetError());
        return 1;
    }
    int axes = SDL_JoystickNumAxes(joy), hats = SDL_JoystickNumHats(joy),
        buttons = SDL_JoystickNumButtons(joy);
    printf("%s: %d axes, %d hats, %d buttons\n", SDL_JoystickName(0), axes, hats, buttons);
    fflush(stdout);

    int prev_axes[32] = {0}, prev_hats[8] = {0}, prev_buttons[64] = {0};
    Uint32 end = SDL_GetTicks() + seconds * 1000;
    while (SDL_GetTicks() < end) {
        SDL_JoystickUpdate();
        for (int i = 0; i < axes && i < 32; i++) {
            int v = SDL_JoystickGetAxis(joy, i);
            if (v != prev_axes[i])
                printf("axis %d = %d\n", i, prev_axes[i] = v);
        }
        for (int i = 0; i < hats && i < 8; i++) {
            int v = SDL_JoystickGetHat(joy, i);
            if (v != prev_hats[i])
                printf("hat %d = %d\n", i, prev_hats[i] = v);
        }
        for (int i = 0; i < buttons && i < 64; i++) {
            int v = SDL_JoystickGetButton(joy, i);
            if (v != prev_buttons[i])
                printf("button %d = %d\n", i, prev_buttons[i] = v);
        }
        fflush(stdout);
        SDL_Delay(10);
    }
    SDL_Quit();
    return 0;
}
