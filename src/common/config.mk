ifeq (,$(BUILD_DIR))
BUILD_DIR=$(shell pwd -P)
endif

ifeq (,$(VERSION))
VERSION="4.x.x-dev-test"
endif

PLATFORM ?= $(UNION_PLATFORM)
ifeq (,$(PLATFORM))
PLATFORM=linux
endif

LIB = /mnt/SDCARD/.tmp_update/lib

CC 		= $(CROSS_COMPILE)gcc
CXX 	= $(CROSS_COMPILE)g++
STRIP 	= $(CROSS_COMPILE)strip

SOURCES := $(SOURCES) .
ifeq ($(INCLUDE_CJSON),1)
SOURCES := $(SOURCES) ../../include/cjson
endif
ifneq ($(INCLUDE_UTILS),0)
CFILES := $(CFILES) \
	../common/utils/str.c \
	../common/utils/log.c \
	../common/utils/file.c
endif
CFILES := $(CFILES) $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))
CPPFILES := $(CPPFILES) $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.cpp))
OFILES = $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

CFLAGS := -I../../include -I../common -DPLATFORM_$(shell echo $(PLATFORM) | tr a-z A-Z) -DONION_VERSION="\"$(VERSION)\"" -Wall

ifeq ($(DEBUG),1)
CFLAGS := $(CFLAGS) -DLOG_DEBUG -g3
endif

ifeq ($(TEST),1)
CFLAGS := $(CFLAGS) -I../include -I../src/common -I$(GTEST_INCLUDE_DIR)
endif

ifeq ($(SANITIZE),1)
CFILES := $(CFILES) ../common/utils/asan.c
CFLAGS := $(CFLAGS) -fno-omit-frame-pointer -fsanitize=address -static-libasan
LDFLAGS := -fsanitize=address -static-libasan $(LDFLAGS)
endif

CXXFLAGS := $(CFLAGS)
LDFLAGS := $(LDFLAGS) -L../../lib -L/usr/local/lib

ifeq ($(PLATFORM),miyoomini)
CFLAGS := $(CFLAGS) -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve -Wl,-rpath=$(LIB)

ifdef INCLUDE_SHMVAR
LDFLAGS := $(LDFLAGS) -lshmvar
endif

endif

# RG35XX (original): Cortex-A9 has no hardware divide and the prebuilt armv7ve
# libraries in ../../lib may use it, so link the rebuilt ones in lib/rg35xx
ifeq ($(PLATFORM),rg35xx)
CFLAGS := $(CFLAGS) -marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -Wl,-rpath=$(LIB)
LDFLAGS := -L../../lib/rg35xx $(filter-out -L../../lib,$(LDFLAGS))

ifdef INCLUDE_SHMVAR
LDFLAGS := $(LDFLAGS) -lshmvar
endif

endif
