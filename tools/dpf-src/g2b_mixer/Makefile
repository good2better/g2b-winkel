#!/usr/bin/make -f
NAME = g2b_mixer
FILES_DSP = MixerPlugin.cpp
FILES_UI  = MixerUI.cpp
EXTRA_LIBS += -lebur128
JACK_LIBS += -lebur128
include ../../Makefile.plugins.mk
# mingw heeft geen systeem-libebur128: vendored .a in vendor/lib
ifeq ($(WINDOWS),true)
EXTRA_LIBS += -Lvendor/lib
endif
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
