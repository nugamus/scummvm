MODULE := engines/peintre

MODULE_OBJS = \
	accueil.o \
	bfg.o \
	gfx.o \
	metaengine.o \
	movie.o \
	obj3d.o \
	peintre.o \
	players.o \
	render3d.o \
	shell.o \
	sound.o \
	world.o

# This module can be built as a plugin
ifeq ($(ENABLE_PEINTRE), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
