MODULE := engines/grumpa

MODULE_OBJS = \
	grumpa.o \
	metaengine.o \
	scene.o \
	render3d.o \
	menu.o

# This module can be built as a plugin
ifeq ($(ENABLE_GRUMPA), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
