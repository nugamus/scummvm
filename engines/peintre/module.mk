MODULE := engines/peintre

MODULE_OBJS = \
	bfg.o \
	metaengine.o \
	movie.o \
	obj3d.o \
	peintre.o

# This module can be built as a plugin
ifeq ($(ENABLE_PEINTRE), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
