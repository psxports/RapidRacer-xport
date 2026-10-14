PROJECT_ROOT := $(abspath ../../..)
SHORT_NAME := RR
FULL_NAME := Rapid Racer (xport)
CONFIG ?= Release
APP_SOURCES := $(wildcard $(PROJECT_ROOT)/src/*.c)
APP_DEFINES := -DXPORT_NATIVE_GPU_CD -DWND_TITLE='"$(FULL_NAME)"' -DWND_WIDTH=960 -DWND_HEIGHT=720 -DFIELD_RATE=50
ifeq ($(CONFIG),Release)
APP_SOURCES := $(filter-out $(PROJECT_ROOT)/src/editor_vis.c,$(APP_SOURCES))
APP_DEFINES += -DXPORT_DISABLE_FUNCTION_MARKERS
endif
include ../../../../xport/src/platform/web/Makefile
