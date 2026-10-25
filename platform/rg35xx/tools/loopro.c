// loopro: attaches a file to a loop device read-only.
// The stock busybox (1.14) only sets up read-write loops, and a file open for
// writing keeps the FAT partition from being remounted read-only at shutdown.
// Static, as it runs in the stock system before the chroot.
// Usage: loopro /dev/block/loopN file

#include <fcntl.h>
#include <linux/loop.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: loopro LOOPDEV FILE\n");
        return 2;
    }

    // The kernel makes the loop read-only when the file is not open for writing
    int file = open(argv[2], O_RDONLY);
    int loop = open(argv[1], O_RDONLY);
    if (file < 0 || loop < 0) {
        perror("loopro: open");
        return 1;
    }
    if (ioctl(loop, LOOP_SET_FD, file) < 0) {
        perror("loopro: LOOP_SET_FD");
        return 1;
    }
    close(file);
    close(loop);
    return 0;
}
