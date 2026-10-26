#ifndef DISPLAY_H__
#define DISPLAY_H__

#include <linux/fb.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>

#include "system.h"
#include "utils/file.h"
#include "utils/log.h"

#ifdef PLATFORM_DEVICE
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#else
#define DEFAULT_WIDTH 752
#define DEFAULT_HEIGHT 560
#endif

// The Miyoo Mini panel is mounted upside down
#ifdef PLATFORM_RG35XX
#define DISPLAY_ROTATED_180 false
#else
#define DISPLAY_ROTATED_180 true
#endif

#define display_on() display_setScreen(true)
#define display_off() display_setScreen(false)

static int fb_fd;
static int DISPLAY_WIDTH = DEFAULT_WIDTH; // physical screen resolution
static int DISPLAY_HEIGHT = DEFAULT_HEIGHT;
struct timeval start_time, end_time;

typedef struct Display {
    long fb_size;
    int width;
    int height;
    int bpp;
    int stride;
    uint32_t *fb_addr;
    uint8_t *fb_ofs;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;
    uint8_t *savebuf;
    bool enabled;
    bool init_done;
} display_t;

static display_t g_display = {
    .width = DEFAULT_WIDTH,
    .height = DEFAULT_HEIGHT,
    .bpp = 32,
    .stride = 0,
    .fb_size = 0,
    .fb_addr = NULL,
    .fb_ofs = NULL,
    .enabled = true,
    .init_done = false,
};

typedef struct Rect {
    int x;
    int y;
    int w;
    int h;
} rect_t;

// Converts a 0xAARRGGBB color to the framebuffer pixel layout
static inline uint32_t display_fbColor(uint32_t argb)
{
#ifdef PLATFORM_RG35XX
    // owlfb stores red in the low byte (0xAABBGGRR)
    return (argb & 0xFF00FF00) | ((argb >> 16) & 0xFF) | ((argb & 0xFF) << 16);
#else
    return argb;
#endif
}

void display_clear(void)
{
    if (g_display.fb_addr) {
        memset(g_display.fb_addr, 0, g_display.fb_size);
    }
}

void display_reset(void)
{
    if (fb_fd < 0)
        fb_fd = open("/dev/fb0", O_RDWR);
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &g_display.vinfo);
    g_display.vinfo.yoffset = 0;
    ioctl(fb_fd, FBIOPUT_VSCREENINFO, &g_display.vinfo);
}

//
//    Get render resolution
//
void display_getRenderResolution()
{
    if (fb_fd < 0)
        fb_fd = open("/dev/fb0", O_RDWR);
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &g_display.vinfo) == 0) {
        g_display.width = g_display.vinfo.xres;
        g_display.height = g_display.vinfo.yres;
    }
    printf_debug("Render resolution: %dx%d\n", g_display.width, g_display.height);
}

//
//    Get physical screen resolution
//
void display_getResolution(void)
{
    FILE *file = fopen("/tmp/screen_resolution", "r");
    if (file == NULL) {
        printf("Failed to open screen resolution file\n");
        return;
    }
    if (fscanf(file, "%dx%d", &DISPLAY_WIDTH, &DISPLAY_HEIGHT) != 2)
        printf("Failed to get screen resolution\n");
    fclose(file);
}

#ifdef PLATFORM_RG35XX
// Puts fb0 back in the panel mode. SDL programs restore the mode they found
// when they exit, and with RetroArch (320x240), GameSwitcher and MainUI
// opening on top of each other the last one out may leave the wrong mode,
// which the display controller then shows scaled.
void display_setPanelMode(void)
{
    struct fb_var_screeninfo vinfo;
    int fd = open("/dev/fb0", O_RDWR);

    if (fd < 0)
        return;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
        vinfo.xres = vinfo.xres_virtual = DISPLAY_WIDTH;
        vinfo.yres = DISPLAY_HEIGHT;
        vinfo.yres_virtual = DISPLAY_HEIGHT * 2;
        vinfo.xoffset = vinfo.yoffset = 0;
        ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
    }
    close(fd);
}
#endif

void display_init(bool map_fb)
{
    if (g_display.init_done)
        return;

    // Open and mmap FB
    fb_fd = open("/dev/fb0", O_RDWR);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &g_display.finfo);

    display_reset();

    if (map_fb) {
        g_display.fb_size = g_display.finfo.smem_len;
        g_display.fb_addr = (uint32_t *)mmap(0, g_display.fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    }
    else {
        g_display.fb_addr = NULL;
    }

    display_getResolution();
    display_getRenderResolution();

    g_display.init_done = true;
}

//
//    Save/Clear Display area
//
void display_save(void)
{
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &g_display.vinfo);
    g_display.bpp = g_display.vinfo.bits_per_pixel / 8; // byte per pixel
    g_display.stride = g_display.vinfo.xres_virtual * g_display.bpp;
    g_display.fb_ofs = (uint8_t *)g_display.fb_addr + (g_display.vinfo.yoffset * g_display.stride);

    // Save display area and clear
    if ((g_display.savebuf = (uint8_t *)malloc(g_display.width * g_display.bpp * g_display.height))) {
        uint32_t i, ofss, ofsd;
        ofss = ofsd = 0;
        for (i = g_display.height; i > 0;
             i--, ofss += g_display.stride, ofsd += g_display.width * g_display.bpp) {
            memcpy(g_display.savebuf + ofsd, g_display.fb_ofs + ofss, g_display.width * g_display.bpp);
            memset(g_display.fb_ofs + ofss, 0, g_display.width * g_display.bpp);
        }
    }
}

//
//    Restore Display area
//
void display_restore(void)
{
    // Restore display area
    if (g_display.savebuf) {
        uint32_t i, ofss, ofsd;
        ofss = ofsd = 0;
        for (i = g_display.height; i > 0;
             i--, ofsd += g_display.stride, ofss += g_display.width * g_display.bpp) {
            memcpy(g_display.fb_ofs + ofsd, g_display.savebuf + ofss, g_display.width * g_display.bpp);
        }
        free(g_display.savebuf);
        g_display.savebuf = NULL;
    }
}

void display_free(display_t *display)
{
    if (display->savebuf) {
        free(display->savebuf);
        display->savebuf = NULL;
    }
    if (display->fb_addr) {
        munmap(display->fb_addr, display->fb_size);
        display->fb_addr = NULL;
    }
}

//
//    Screen On/Off
//
void display_setScreen(bool enabled)
{
#ifdef PLATFORM_RG35XX
    // FB_BLANK_UNBLANK / FB_BLANK_POWERDOWN
    file_write(BACKLIGHT_DIR "bl_power", enabled ? "0" : "4", 1);

    if (enabled)
        display_restore();
    else
        display_save();
    g_display.enabled = enabled;
    return;
#endif

    // export gpio4, direction: out
    file_write(GPIO_DIR1 "export", "4", 1);
    file_write(GPIO_DIR2 "gpio4/direction", "out", 3);

    // screen on/off
    file_write(GPIO_DIR2 "gpio4/value", enabled ? "1" : "0", 1);

    // unexport gpio4
    file_write(GPIO_DIR1 "unexport", "4", 1);

    if (enabled) {
        // re-enable brightness control
        file_write(PWM_DIR "export", "0", 1);
        file_write(PWM_DIR "pwm0/enable", "0", 1);
        file_write(PWM_DIR "pwm0/enable", "1", 1);
        display_restore();
    }
    else {
        display_save();
    }
    g_display.enabled = enabled;
}

void display_toggle(void) { display_setScreen(!g_display.enabled); }

uint32_t display_getBrightnessRaw()
{
    uint32_t duty_cycle = 0;
    FILE *fp;

#ifdef PLATFORM_RG35XX
    // Scaled to the 0-100 duty cycle range used by the Miyoo curve below
    uint32_t brightness = 0;
    file_get(fp, BACKLIGHT_DIR "brightness", "%u", &brightness);
    return round(brightness * 100.0 / BACKLIGHT_MAX);
#endif

    if (exists(PWM_DIR "pwm0/duty_cycle")) {
        file_get(fp, PWM_DIR "pwm0/duty_cycle", "%u", &duty_cycle);
    }
    return duty_cycle;
}

// Get display brightness from raw (0 - 10)
int display_getBrightnessFromRaw()
{
    int value_raw = display_getBrightnessRaw();
    int value = round((log(value_raw / 3.0) / 0.350656));
    return value;
}
//
//    Set Brightness (Raw)
//
void display_setBrightnessRaw(uint32_t value)
{
    FILE *fp;
#ifdef PLATFORM_RG35XX
    // value is a 0-100 duty cycle, as on the Miyoo
    file_put_sync(fp, BACKLIGHT_DIR "brightness", "%u", (uint32_t)round(value * BACKLIGHT_MAX / 100.0));
#else
    file_put_sync(fp, PWM_DIR "pwm0/duty_cycle", "%u", value);
#endif
    printf_debug("Raw brightness: %d\n", value);
}

// Set display brightness (0 - 10)
void display_setBrightness(uint32_t value)
{
    // Linear curve
    // int value_raw = (value == 0) ? 3 : (value * 10);

    // Exponential curve
    int value_raw = round(3.0 * exp(0.350656 * value));

    display_setBrightnessRaw(value_raw);
}

/**
 * @brief Read from or write to a framebuffer buffer.
 *
 * This function reads from or writes to a specific buffer in the framebuffer.
 * It supports optional rotation of the buffer content.
 * Pixel data is 0xAARRGGBB and is converted to the framebuffer layout.
 *
 * @param index The index of the buffer to read/write.
 * @param display The display structure.
 * @param pixels Pointer to the pixel data.
 * @param rect The rectangle area to read/write.
 * @param rotate Whether to rotate the buffer content.
 * @param write Whether to write to the buffer (true) or read from it (false).
 * @param mask Whether to use a mask when writing to the buffer.
 */
void display_readOrWriteBuffer(int index, display_t *display, uint32_t *pixels, rect_t rect, bool rotate, bool mask, bool write)
{
    int bufferPos = index * display->vinfo.yres;

    for (int oy = 0; oy < rect.h; oy++) {
        int y = rect.y + oy;

        if (y < 0 || y >= display->vinfo.yres)
            continue;

        int virtualY = bufferPos + (rotate ? (display->vinfo.yres - 1) - y : y);
        long baseOffset = (long)virtualY * display->vinfo.xres;
        int baseIndex = oy * rect.w;

        for (int ox = 0; ox < rect.w; ox++) {
            int x = rect.x + ox;

            if (rotate) {
                x = (display->vinfo.xres - 1) - x;
            }

            if (x < 0 || x >= display->vinfo.xres)
                continue;

            long offset = baseOffset + (long)x;
            int index = baseIndex + ox;
            if (write) {
                if (mask) {
                    if (pixels[index] != 0) {
                        display->fb_addr[offset] = 0;
                    }
                }
                else {
                    display->fb_addr[offset] = display_fbColor(pixels[index]);
                }
            }
            else {
                if (mask) {
                    pixels[index] = display->fb_addr[offset] == 0 ? 1 : 0;
                }
                else {
                    // display_fbColor only swaps red and blue, so it also converts back
                    pixels[index] = display_fbColor(display->fb_addr[offset]);
                }
            }
        }
    }
}

/**
 * @brief Read from the current framebuffer buffer.
 * 
 * This function reads from the current buffer in the framebuffer.
 * It supports optional rotation of the buffer content.
 * 
 * @param display The display structure.
 * @param pixels Pointer to the pixel data.
 * @param rect The rectangle area to read/write.
 * @param rotate Whether to rotate the buffer content.
 * @param mask Whether to use a mask when writing to the buffer.
 */
void display_readCurrentBuffer(display_t *display, uint32_t *pixels, rect_t rect, bool rotate, bool mask)
{
    int index = display->vinfo.yoffset / display->vinfo.yres;
    display_readOrWriteBuffer(index, display, pixels, rect, rotate, mask, false);
}

/** 
 * @brief Read from a framebuffer buffer.
 *
 * This function reads from a specific buffer in the framebuffer.
 * It supports optional rotation of the buffer content.
 *
 * @param index The index of the buffer to read.
 * @param display The display structure.
 * @param pixels Pointer to the pixel data.
 * @param rect The rectangle area to read/write.
 * @param rotate Whether to rotate the buffer content.
 * @param mask Whether to use a mask when writing to the buffer.
 */
void display_readBuffer(int index, display_t *display, uint32_t *pixels, rect_t rect, bool rotate, bool mask)
{
    display_readOrWriteBuffer(index, display, pixels, rect, rotate, mask, false);
}

/** 
 * @brief Write to a framebuffer buffer.
 *
 * This function writes to a specific buffer in the framebuffer.
 * It supports optional rotation of the buffer content.
 *
 * @param index The index of the buffer to write.
 * @param display The display structure.
 * @param pixels Pointer to the pixel data.
 * @param rect The rectangle area to read/write.
 * @param rotate Whether to rotate the buffer content.
 * @param mask Whether to use a mask when writing to the buffer.
 */
void display_writeBuffer(int index, display_t *display, uint32_t *pixels, rect_t rect, bool rotate, bool mask)
{
    display_readOrWriteBuffer(index, display, pixels, rect, rotate, mask, true);
}

/**
 * @brief Read from or write to multiple framebuffer buffers.
 *
 * This function reads from or writes to multiple buffers in the framebuffer.
 * It supports optional rotation of the buffer content.
 *
 * @param display The display structure.
 * @param pixels Array of pointers to the pixel data for each buffer.
 * @param rect The rectangle area to read/write.
 * @param rotate Whether to rotate the buffer content.
 * @param write Whether to write to the buffers (true) or read from them (false).
 * @param mask Whether to use a mask when writing to the buffers.
 */
void display_readOrWriteBuffers(display_t *display, uint32_t **pixels, rect_t rect, bool rotate, bool mask, bool write)
{
    int numBuffers = display->vinfo.yres_virtual / display->vinfo.yres;

    for (int b = 0; b < numBuffers; b++) {
        display_readOrWriteBuffer(b, display, pixels[b], rect, rotate, mask, write);
    }
}

/** 
 * @brief Read from multiple framebuffer buffers.
 *
 * This function reads from multiple buffers in the framebuffer.
 * It supports optional rotation of the buffer content.
 *
 * @param display The display structure.
 * @param pixels Array of pointers to the pixel data for each buffer.
 * @param rect The rectangle area to read/write.
 * @param rotate Whether to rotate the buffer content.
 * @param mask Whether to use a mask when writing to the buffers.
 */
void display_readBuffers(display_t *display, uint32_t **pixels, rect_t rect, bool rotate, bool mask)
{
    display_readOrWriteBuffers(display, pixels, rect, rotate, mask, false);
}

/** 
 * @brief Write to multiple framebuffer buffers.
 *
 * This function writes to multiple buffers in the framebuffer.
 * It supports optional rotation of the buffer content.
 *
 * @param display The display structure.
 * @param pixels Array of pointers to the pixel data for each buffer.
 * @param rect The rectangle area to read/write.
 * @param rotate Whether to rotate the buffer content.
 * @param mask Whether to use a mask when writing to the buffers.
 */
void display_writeBuffers(display_t *display, uint32_t **pixels, rect_t rect, bool rotate, bool mask)
{
    display_readOrWriteBuffers(display, pixels, rect, rotate, mask, true);
}

//
//    Draw frame, fixed 640x480x32bpp for now
//
void display_drawFrame(uint32_t color)
{
    color = display_fbColor(color);
    uint32_t *ofs = g_display.fb_addr;
    uint32_t i;
    for (i = 0; i < 640; i++) {
        ofs[i] = color;
    }
    ofs += 640 * 479;
    for (i = 0; i < 640 * 2; i++) {
        ofs[i] = color;
    }
    ofs += 640 * 480;
    for (i = 0; i < 640 * 2; i++) {
        ofs[i] = color;
    }
    ofs += 640 * 480;
    for (i = 0; i < 640; i++) {
        ofs[i] = color;
    }
    ofs = g_display.fb_addr + 639;
    for (i = 0; i < 480 * 3 - 1; i++, ofs += 640) {
        ofs[0] = color;
        ofs[1] = color;
    }
}

//
//    Draw a battery icon
//
void display_drawBatteryIcon(uint32_t color, int x, int y, int level,
                             uint32_t fillColor)
{
    color = display_fbColor(color);
    fillColor = display_fbColor(fillColor);
    uint32_t *ofs = g_display.fb_addr;
    int i, j;

    // Draw battery body wireframe
    for (i = x; i < x + 30; i++) {
        ofs[i + y * g_display.width] = color;        // Top border
        ofs[i + (y + 14) * g_display.width] = color; // Bottom border
    }
    for (j = y; j < y + 15; j++) {
        ofs[x + j * g_display.width] = color;      // Left border
        ofs[x + 29 + j * g_display.width] = color; // Right border
    }

    // Draw battery charge level
    int levelWidth = (level * 26) / 100;
    for (i = x + 3 + 26 - levelWidth; i < x + 1 + 26; i++) {
        for (j = y + 3; j < y + 12; j++) {
            ofs[i + j * g_display.width] = fillColor;
        }
    }

    // Draw battery head wireframe
    for (i = x - 4; i < x; i++) {
        for (j = y + 2; j < y + 13; j++) {
            ofs[i + j * g_display.width] = color;
        }
    }
}

void display_close(void)
{
    display_reset();
    display_free(&g_display);

    if (fb_fd > 0)
        close(fb_fd);
}

#endif // DISPLAY_H__
