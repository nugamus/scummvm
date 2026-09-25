MODULE := engines/x3d

MODULE_OBJS = \
	a3d.o \
	collision.o \
	dmf.o \
	interaction.o \
	inventory.o \
	metaengine.o \
	o3d.o \
	player.o \
	renderer.o \
	renderer_opengl.o \
	renderer_tinygl.o \
	scene.o \
	sound.o \
	talk.o \
	u01.o \
	x3d.o

# This module can be built as a plugin
ifeq ($(ENABLE_X3D), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
