MODULE := engines/ring

MODULE_OBJS = \
	bag.o \
	codec.o \
	cursor.o \
	metaengine.o \
	movie.o \
	resources.o \
	ring.o \
	rotation.o \
	sound.o \
	world.o \
	ring/as.o \
	ring/ni.o \
	ring/setup.o \
	ring/sy.o

# This module can be built as a plugin
ifeq ($(ENABLE_RING), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
