#ifndef UTILS_PADSP_H__
#define UTILS_PADSP_H__

// Miyoo libpadsp routes OSS audio to audioserver; the RG35XX uses ALSA
#ifdef PLATFORM_MIYOOMINI
#define PADSP_PRELOAD "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so "
#else
#define PADSP_PRELOAD ""
#endif

#endif // UTILS_PADSP_H__
