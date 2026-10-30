// vtreset: recovers the console after an SDL 1.2 fbcon program dies.
// (The RG35XX SDL is patched to stay on the active VT; this is a safety net.)
// SDL fbcon switches to a free VT, puts it in KD_GRAPHICS and VT_PROCESS mode
// and only switches back on a clean exit. After a crash that VT stays active
// in that state and the next SDL program waits forever in VT_WAITACTIVE.
// This puts the active VT back in auto mode and activates the target VT. The
// VT stays in graphics mode: back in text mode fbcon would paint the empty
// console over the screen (the boot logo, at boot).
// Usage: vtreset [vt]   (default 1)

#include <fcntl.h>
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
        // A switch requested while the owner was dying waits for an
        // acknowledgement that never comes; give it (fails if none is pending)
        ioctl(active, VT_RELDISP, 1);
        if (ioctl(active, VT_SETMODE, &mode) < 0)
            perror("vtreset: VT_SETMODE");
        close(active);
    }

    // SDL fbcon locks VT switching while in graphics mode and only unlocks on
    // a clean exit; while locked every VT_ACTIVATE is ignored
    ioctl(tty0, VT_UNLOCKSWITCH, 1);

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
