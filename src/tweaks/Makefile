INCLUDE_SHMVAR=1
INCLUDE_CJSON=1
include ../common/config.mk

TARGET = tweaks
CFLAGS := $(CFLAGS) -D_DEFAULT_SOURCE -DHAS_AUDIO
LDFLAGS := $(LDFLAGS) -lSDL -lSDL_ttf -lSDL_image -lSDL_mixer -lSDL_rotozoom -pthread
# libkbinput is built from third-party/SearchFilter, not available for rg35xx yet
ifneq ($(PLATFORM),rg35xx)
LDFLAGS := $(LDFLAGS) -lkbinput
endif

include ../common/commands.mk
include ../common/recipes.mk
