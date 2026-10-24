#!/usr/bin/make -f
NAME = g2b_masterrack
FILES_DSP = MasterRackPlugin.cpp
FILES_UI  = MasterRackUI.cpp
EXTRA_LIBS += -lebur128
JACK_LIBS += -lebur128
EXTRA_LIBS += -Lvendor/lib
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
