INCLUDE_CJSON=1
include ../common/config.mk

TARGET = MainUI
CFLAGS := $(CFLAGS) -DHAS_AUDIO
LDFLAGS := $(LDFLAGS) -lSDL -lSDL_ttf -lSDL_image -lSDL_mixer -lSDL_rotozoom -pthread

include ../common/commands.mk
include ../common/recipes.mk
