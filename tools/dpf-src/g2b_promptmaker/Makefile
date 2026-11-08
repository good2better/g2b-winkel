#!/usr/bin/make -f
NAME = g2b_promptmaker
FILES_DSP = PromptmakerPlugin.cpp
FILES_UI  = PromptmakerUI.cpp
BASE_OPTS += -fno-lto
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
