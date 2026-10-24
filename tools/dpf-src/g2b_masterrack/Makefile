#!/usr/bin/make -f
NAME = g2b_masterrack
FILES_DSP = MasterRackPlugin.cpp
FILES_UI  = MasterRackUI.cpp
EXTRA_LIBS += -lebur128
JACK_LIBS += -lebur128
EXTRA_LIBS += -Lvendor/lib
include ../../Makefile.plugins.mk
# mingw heeft geen systeem-libebur128: vendored .a in vendor/lib
ifeq ($(WINDOWS),true)
EXTRA_LIBS += -Lvendor/lib
endif
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
