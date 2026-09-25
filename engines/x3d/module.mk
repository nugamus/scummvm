MODULE := engines/x3d

MODULE_OBJS = \
	a3d.o \
	collision.o \
	interaction.o \
	dmf.o \
	metaengine.o \
	o3d.o \
	player.o \
	scene.o \
	x3d.o

# This module can be built as a plugin
ifeq ($(ENABLE_X3D), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
