MODULE := engines/grumpa

MODULE_OBJS = \
	grumpa.o \
	console.o \
	metaengine.o \
	scene.o \
	render3d.o \
	menu.o \
	inventory.o \
	saveload.o \
	events.o \
	movie.o \
	character.o \
	combat.o \
	follower.o \
	dialogue.o \
	score.o \
	items.o \
	walk.o

# The scummvm-agent-bridge adapter (BRIDGE commit, agent-bridge branch only).
MODULE_OBJS += agent.o

# This module can be built as a plugin
ifeq ($(ENABLE_GRUMPA), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
