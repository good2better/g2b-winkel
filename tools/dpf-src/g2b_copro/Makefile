#!/usr/bin/make -f
NAME = g2b_copro
FILES_DSP = CoproPlugin.cpp vendor/MidiFile.cpp vendor/MidiMessage.cpp vendor/MidiEvent.cpp vendor/MidiEventList.cpp vendor/Binasc.cpp vendor/Options.cpp
FILES_UI  = CoproUI.cpp
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
