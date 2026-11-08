#!/usr/bin/make -f
NAME = g2b_dicesynth
FILES_DSP = DiceSynthPlugin.cpp
FILES_UI  = DiceSynthUI.cpp
BASE_OPTS += -fno-lto
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
