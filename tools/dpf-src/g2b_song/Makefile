#!/usr/bin/make -f
# Makefile for G2B SONG (DISTRHO Plugin Framework)

NAME = g2b_song

FILES_DSP = SongPlugin.cpp vendor/MidiFile.cpp vendor/MidiMessage.cpp vendor/MidiEvent.cpp vendor/MidiEventList.cpp vendor/Binasc.cpp vendor/Options.cpp
FILES_UI  = SongUI.cpp

include ../../Makefile.plugins.mk

TARGETS += vst3
TARGETS += clap
TARGETS += jack

all: $(TARGETS)
