MODULE := engines/ring

MODULE_OBJS = 	codec.o 	metaengine.o 	movie.o 	resources.o 	ring.o

# This module can be built as a plugin
ifeq ($(ENABLE_RING), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
