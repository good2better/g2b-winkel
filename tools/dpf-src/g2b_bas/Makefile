#!/usr/bin/make -f
NAME = g2b_bas
FILES_DSP = BasPlugin.cpp
FILES_UI  = BasUI.cpp
# LTO miscompiles this plugin (segfault in UI) — build without it
BASE_OPTS += -fno-lto
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
