MODULE := engines/gilbert

MODULE_OBJS = \
	collection.o \
	cua.o \
	database.o \
	dialog.o \
	gilbert.o \
	logic.o \
	menu.o \
	metaengine.o \
	room.o \
	sound.o

# This module can be built as a plugin
ifeq ($(ENABLE_GILBERT), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
