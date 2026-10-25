// vtreset: recovers the console after an SDL 1.2 fbcon program dies.
// SDL fbcon switches to a free VT, puts it in KD_GRAPHICS and VT_PROCESS mode
// and only switches back on a clean exit. After a crash that VT stays active
// in that state and the next SDL program waits forever in VT_WAITACTIVE.
// This puts the active VT back in text/auto mode and activates the target VT.
// Usage: vtreset [vt]   (default 1)

#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int target = argc > 1 ? atoi(argv[1]) : 1;
    struct vt_stat state;
    struct vt_mode mode = {.mode = VT_AUTO};
    char path[32];

    int tty0 = open("/dev/tty0", O_RDWR);
    if (tty0 < 0 || ioctl(tty0, VT_GETSTATE, &state) < 0) {
        perror("vtreset: tty0");
        return 1;
    }
    printf("active vt: %d\n", state.v_active);

    snprintf(path, sizeof(path), "/dev/tty%d", state.v_active);
    int active = open(path, O_RDWR);
    if (active >= 0) {
        if (ioctl(active, VT_SETMODE, &mode) < 0)
            perror("vtreset: VT_SETMODE");
        if (ioctl(active, KDSETMODE, KD_TEXT) < 0)
            perror("vtreset: KDSETMODE");
        close(active);
    }

    if (state.v_active != target) {
        if (ioctl(tty0, VT_ACTIVATE, target) < 0 ||
            ioctl(tty0, VT_WAITACTIVE, target) < 0) {
            perror("vtreset: VT_ACTIVATE");
            return 1;
        }
    }

    ioctl(tty0, VT_GETSTATE, &state);
    printf("active vt: %d\n", state.v_active);
    close(tty0);
    return 0;
}
