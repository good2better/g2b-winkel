#!/usr/bin/make -f
NAME = g2b_fx
FILES_DSP = FxPlugin.cpp
FILES_UI  = FxUI.cpp
EXTRA_LIBS += -lebur128
JACK_LIBS += -lebur128
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
