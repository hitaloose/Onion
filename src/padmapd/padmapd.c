// padmapd: RG35XX input translator.
// Reads the "RG35XX Gamepad" (EV_KEY BTN_*, d-pad on ABS_HAT0, L2/R2 on
// ABS_Z/ABS_RZ) and the power key (atc260x_onoff), and re-emits them through
// uinput as a keyboard with the Miyoo Mini keycodes (system/keymap_hw.h).
// The kernel keyboard handler feeds this device to the active VT, so SDL 1.2
// fbcon receives the same keys as on the Miyoo, and Onion tools can read the
// virtual device through the INPUT_SYMLINK path.
// The source devices are not grabbed, so /dev/input/js0 keeps working.

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "system/keymap_hw.h"

#define DEVICE_NAME "Onion Keyboard"
#define INPUT_SYMLINK "/dev/input/onion-kbd"
#define GAMEPAD_NAME "RG35XX Gamepad"
#define POWER_NAME "atc260x_onoff"
#define TRIGGER_THRESHOLD 127

static const struct {
    int src;
    int dst;
} button_map[] = {
    {BTN_A, HW_BTN_A},
    {BTN_B, HW_BTN_B},
    {BTN_X, HW_BTN_X},
    {BTN_Y, HW_BTN_Y},
    {BTN_TL, HW_BTN_L1},
    {BTN_TR, HW_BTN_R1},
    {BTN_SELECT, HW_BTN_SELECT},
    {BTN_START, HW_BTN_START},
    {BTN_MODE, HW_BTN_MENU},
    {BTN_THUMBL, HW_BTN_VOLUME_UP},
    {BTN_THUMBR, HW_BTN_VOLUME_DOWN},
    // BTN_Z fires together with KEY_POWER and is ignored
};

static const int output_keys[] = {
    HW_BTN_UP, HW_BTN_DOWN, HW_BTN_LEFT, HW_BTN_RIGHT,
    HW_BTN_A, HW_BTN_B, HW_BTN_X, HW_BTN_Y,
    HW_BTN_L1, HW_BTN_R1, HW_BTN_L2, HW_BTN_R2,
    HW_BTN_SELECT, HW_BTN_START, HW_BTN_MENU, HW_BTN_POWER,
    HW_BTN_VOLUME_UP, HW_BTN_VOLUME_DOWN};

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static volatile sig_atomic_t quit = 0;

static void on_signal(int sig) { quit = 1; }

// Opens the /dev/input/event* device whose name matches, or returns -1
static int open_input_by_name(const char *name)
{
    DIR *dir = opendir("/dev/input");
    struct dirent *entry;
    char path[300], dev_name[256];
    int fd = -1;

    if (!dir)
        return -1;

    while (fd < 0 && (entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        int cur = open(path, O_RDONLY | O_NONBLOCK);
        if (cur < 0)
            continue;
        memset(dev_name, 0, sizeof(dev_name));
        if (ioctl(cur, EVIOCGNAME(sizeof(dev_name) - 1), dev_name) >= 0 &&
            strcmp(dev_name, name) == 0)
            fd = cur;
        else
            close(cur);
    }

    closedir(dir);
    return fd;
}

// Kernel 3.10 has no UI_GET_SYSNAME, so find our event node through sysfs
static int find_event_node(const char *name, char *out, size_t out_len)
{
    DIR *dir = opendir("/sys/class/input");
    struct dirent *entry;
    char path[300], dev_name[256];
    int found = 0;

    if (!dir)
        return 0;

    while (!found && (entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;
        snprintf(path, sizeof(path), "/sys/class/input/%s/device/name", entry->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp)
            continue;
        if (fgets(dev_name, sizeof(dev_name), fp)) {
            dev_name[strcspn(dev_name, "\n")] = 0;
            if (strcmp(dev_name, name) == 0) {
                snprintf(out, out_len, "/dev/input/%s", entry->d_name);
                found = 1;
            }
        }
        fclose(fp);
    }

    closedir(dir);
    return found;
}

static int create_uinput(void)
{
    struct uinput_user_dev dev;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0)
        return -1;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    for (int i = 0; i < ARRAY_LEN(output_keys); i++)
        ioctl(fd, UI_SET_KEYBIT, output_keys[i]);

    memset(&dev, 0, sizeof(dev));
    snprintf(dev.name, UINPUT_MAX_NAME_SIZE, DEVICE_NAME);
    dev.id.bustype = BUS_VIRTUAL;
    dev.id.vendor = 0x0001;
    dev.id.product = 0x0001;
    dev.id.version = 1;

    if (write(fd, &dev, sizeof(dev)) != sizeof(dev) ||
        ioctl(fd, UI_DEV_CREATE) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void emit(int fd, int type, int code, int value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) < 0)
        perror("padmapd: write");
}

static void emit_key(int fd, int code, int value)
{
    emit(fd, EV_KEY, code, value);
    emit(fd, EV_SYN, SYN_REPORT, 0);
}

// Presses or releases a digital key only when its state changes
static void set_key(int fd, int code, int pressed, int *state)
{
    if (*state == pressed)
        return;
    *state = pressed;
    emit_key(fd, code, pressed);
}

int main(int argc, char *argv[])
{
    int out_fd, pad_fd, power_fd;
    int hat_x = 0, hat_y = 0;
    int up = 0, down = 0, left = 0, right = 0, l2 = 0, r2 = 0;
    char node[300];

    pad_fd = open_input_by_name(GAMEPAD_NAME);
    power_fd = open_input_by_name(POWER_NAME);
    if (pad_fd < 0) {
        fprintf(stderr, "padmapd: '%s' not found\n", GAMEPAD_NAME);
        return 1;
    }

    out_fd = create_uinput();
    if (out_fd < 0) {
        perror("padmapd: uinput");
        return 1;
    }

    // Give udev-less /dev a stable name for the virtual keyboard
    usleep(100000);
    if (find_event_node(DEVICE_NAME, node, sizeof(node))) {
        unlink(INPUT_SYMLINK);
        if (symlink(node, INPUT_SYMLINK) < 0)
            perror("padmapd: symlink");
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, SIG_IGN);

    struct pollfd fds[2] = {{pad_fd, POLLIN, 0}, {power_fd, POLLIN, 0}};
    int nfds = power_fd < 0 ? 1 : 2;
    struct input_event ev;

    while (!quit) {
        if (poll(fds, nfds, -1) < 0)
            continue;

        if (nfds == 2 && (fds[1].revents & POLLIN)) {
            while (read(power_fd, &ev, sizeof(ev)) == sizeof(ev))
                if (ev.type == EV_KEY && ev.code == KEY_POWER)
                    emit_key(out_fd, HW_BTN_POWER, ev.value);
        }

        if (!(fds[0].revents & POLLIN))
            continue;

        while (read(pad_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_KEY) {
                for (int i = 0; i < ARRAY_LEN(button_map); i++) {
                    if (button_map[i].src == ev.code) {
                        emit_key(out_fd, button_map[i].dst, ev.value);
                        break;
                    }
                }
            }
            else if (ev.type == EV_ABS) {
                switch (ev.code) {
                case ABS_HAT0X:
                    hat_x = ev.value;
                    set_key(out_fd, HW_BTN_LEFT, hat_x < 0, &left);
                    set_key(out_fd, HW_BTN_RIGHT, hat_x > 0, &right);
                    break;
                case ABS_HAT0Y:
                    hat_y = ev.value;
                    set_key(out_fd, HW_BTN_UP, hat_y < 0, &up);
                    set_key(out_fd, HW_BTN_DOWN, hat_y > 0, &down);
                    break;
                case ABS_Z:
                    set_key(out_fd, HW_BTN_L2, ev.value > TRIGGER_THRESHOLD, &l2);
                    break;
                case ABS_RZ:
                    set_key(out_fd, HW_BTN_R2, ev.value > TRIGGER_THRESHOLD, &r2);
                    break;
                default:
                    break;
                }
            }
        }
    }

    unlink(INPUT_SYMLINK);
    ioctl(out_fd, UI_DEV_DESTROY);
    close(out_fd);
    return 0;
}
