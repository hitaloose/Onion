// The headers in include/png are libpng 1.2, but the RG35XX rootfs ships
// libpng 1.6: use the sysroot header so the code matches the library ABI.
#include <libpng16/png.h>
