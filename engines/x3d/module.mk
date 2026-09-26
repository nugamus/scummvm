MODULE := engines/x3d

MODULE_OBJS = \
	a3d.o \
	collision.o \
	console.o \
	dmf.o \
	frame.o \
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
	x3d.o \
	monet/gallery3d.o \
	monet/menus.o \
	monet/u00.o \
	monet/u01.o \
	monet/u02.o \
	monet/u03.o \
	monet/u04.o \
	monet/u05.o \
	monet/u06.o \
	monet/u07.o \
	monet/u33.o

# This module can be built as a plugin
ifeq ($(ENABLE_X3D), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
