#!/usr/bin/make -f
NAME = g2b_geluidmidi
FILES_DSP = GeluidMidiPlugin.cpp vendor/MidiFile.cpp vendor/MidiMessage.cpp vendor/MidiEvent.cpp vendor/MidiEventList.cpp vendor/Binasc.cpp vendor/Options.cpp
FILES_UI  = GeluidMidiUI.cpp
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
