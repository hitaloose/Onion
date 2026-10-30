// vsynchz: measures the display refresh rate from the owlfb vsync interrupt.
// OWLFB_WAITFORVSYNC returns at once unless the vsync event is enabled
// (OWLFB_VSYNC_EVENT_EN, done by the Android hwcomposer on the stock
// firmware); this enables it, times the waits and disables it again.
// Usage: vsynchz [frames]   (default 300)
// Build: platform/rg35xx/tools/make.sh '${CROSS_COMPILE}gcc -Wall -O2 \
//        platform/rg35xx/tools/vsynchz.c -o cache/rg35xx/bin/vsynchz'

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

struct owlfb_sync_info {
    uint8_t enabled;
    uint8_t disp_id;
    uint16_t reserved2;
};

#define OWLFB_WAITFORVSYNC _IOW('O', 57, long long)
#define OWLFB_VSYNC_EVENT_EN _IOW('O', 67, struct owlfb_sync_info)

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[])
{
    int frames = argc > 1 ? atoi(argv[1]) : 300;
    struct owlfb_sync_info sync = {1, 0, 0};
    long long ts = 0;
    double t0, t1, min = 1, max = 0, prev;
    int fd, i;

    fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("/dev/fb0");
        return 1;
    }
    if (ioctl(fd, OWLFB_VSYNC_EVENT_EN, &sync) < 0)
        perror("OWLFB_VSYNC_EVENT_EN");

    ioctl(fd, OWLFB_WAITFORVSYNC, &ts);
    t0 = prev = now();
    for (i = 0; i < frames; i++) {
        double t, d;
        ioctl(fd, OWLFB_WAITFORVSYNC, &ts);
        t = now();
        d = t - prev;
        if (d < min)
            min = d;
        if (d > max)
            max = d;
        prev = t;
    }
    t1 = now();

    sync.enabled = 0;
    ioctl(fd, OWLFB_VSYNC_EVENT_EN, &sync);
    close(fd);

    printf("%.3f Hz over %d frames (frame %.2f-%.2f ms)\n",
           frames / (t1 - t0), frames, min * 1000, max * 1000);
    return 0;
}
