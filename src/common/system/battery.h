#ifndef BATTERY_H__
#define BATTERY_H__

#include "system/device_model.h"
#include "system/system.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/msleep.h"
#include "utils/process.h"

static time_t battery_last_modified = 0;
static bool battery_is_charging = false;

#ifdef PLATFORM_RG35XX
#define BATTERY_CAPACITY_PATH "/sys/class/power_supply/battery/capacity"
#endif

/**
 * @brief Battery percentage straight from the fuel gauge, -1 when unknown
 *
 * The RG35XX gauge reports 0 until it has measured the battery after boot, and
 * that 0 is not an empty battery: the device just booted on it. Callers that
 * act on a low battery must check battery_hasReading() first.
 *
 * @return int : Battery percentage (1-100) or -1 when there is no measurement
 */
int battery_getGaugePercentage(void)
{
#ifdef PLATFORM_RG35XX
    FILE *fp;
    int percentage = 0;

    file_get(fp, BATTERY_CAPACITY_PATH, "%d", &percentage);

    return percentage > 0 ? percentage : -1;
#else
    return -1;
#endif
}

/**
 * @brief True when battery_getPercentage() is a real reading
 *
 * batmon writes /tmp/percBat as soon as it has a value; before that only the
 * fuel gauge can answer, and only once it has measured.
 */
bool battery_hasReading(void)
{
    if (exists("/tmp/percBat"))
        return true;

    return battery_getGaugePercentage() >= 0;
}

/**
 * @brief Retrieve the current battery percentage as reported by batmon
 *
 * @return int : Battery percentage (0-100) or 500 if charging
 */

int battery_getPercentage(void)
{
    FILE *fp;
    int percentage = -1;
    int retry = 3;

    while (percentage == -1 && retry > 0) {
        if (exists("/tmp/percBat")) {
            file_get(fp, "/tmp/percBat", "%d", &percentage);
            break;
        }
        else {
            printf_debug("/tmp/percBat not found (%d)\n", retry);

            if (!process_isRunning("batmon")) {
                printf_debug("bin/batmon not running (%d)\n", retry);
                break;
            }
        }
        retry--;
        msleep(100);
    }

#ifndef PLATFORM_DEVICE
#ifdef LOG_DEBUG
    return 78;
#endif
#endif

#ifdef PLATFORM_RG35XX
    // Until batmon writes percBat, read the fuel gauge itself: a 0% here
    // makes keymon close the game for low battery and shut down
    if (percentage == -1) {
        int gauge_percentage = battery_getGaugePercentage();
        if (gauge_percentage >= 0)
            percentage = gauge_percentage;
    }
#endif

    if (percentage == -1)
        percentage = 0; // show zero when percBat not found

    return percentage;
}

bool battery_isCharging(void)
{
#ifdef PLATFORM_MIYOOMINI
    if (DEVICE_ID == MIYOO283) {
        char charging = 0;
        int fd = open(GPIO_DIR2 "gpio59/value", O_RDONLY);

        if (fd < 0) {
            // export gpio59, direction: in
            file_write(GPIO_DIR1 "export", "59", 2);
            file_write(GPIO_DIR2 "gpio59/direction", "in", 2);
            fd = open(GPIO_DIR2 "gpio59/value", O_RDONLY);
        }

        if (fd >= 0) {
            read(fd, &charging, 1);
            close(fd);
        }

        return charging == '1';
    }
    else if (DEVICE_ID == MIYOO354) {
        char *cmd = "cd /customer/app/ ; ./axp_test";
        int batJsonSize = 100;
        char buf[batJsonSize];
        int charge_number;

        FILE *fp;
        fp = popen(cmd, "r");
        if (fgets(buf, batJsonSize, fp) != NULL) {
            sscanf(buf, "{\"battery\":%*d, \"voltage\":%*d, \"charging\":%d}",
                   &charge_number);
        }
        pclose(fp);
        return charge_number == 3;
    }
    return false;
#elif defined(PLATFORM_RG35XX)
    FILE *fp;
    int online = 0;
    file_get(fp, "/sys/class/power_supply/battery/charger_online", "%d", &online);
    return online == 1;
#else
    return false;
#endif
}

bool battery_hasChanged(int ticks, int *out_percentage)
{
    bool changed = false;

    if (battery_isCharging()) {
        if (!battery_is_charging) {
            *out_percentage = 500;
            battery_is_charging = true;
            return true;
        }
        return false;
    }
    else if (battery_is_charging) {
        battery_is_charging = false;
    }

    bool refresh = file_isModified("/tmp/percBat", &battery_last_modified);

#ifdef PLATFORM_RG35XX
    // batmon can take a while to write percBat after boot (sqlite start-up).
    // Until it exists, follow the gauge once a second so the header does not
    // stay at 0% while the battery is fine. battery_getPercentage() waits for
    // batmon (up to 300 ms), so read the gauge directly while percBat is
    // missing instead of stalling the UI.
    if (!exists("/tmp/percBat")) {
        static int last_gauge_check = 0;
        if (ticks - last_gauge_check >= 1000) {
            last_gauge_check = ticks;

            int gauge_percentage = battery_getGaugePercentage();
            if (gauge_percentage >= 0 && gauge_percentage != *out_percentage) {
                *out_percentage = gauge_percentage;
                changed = true;
            }
        }
        return changed;
    }
#endif

    if (refresh) {
        int current_percentage = battery_getPercentage();

        if (current_percentage != *out_percentage) {
            *out_percentage = current_percentage;
            changed = true;
        }
    }

    return changed;
}

#endif // BATTERY_H__
