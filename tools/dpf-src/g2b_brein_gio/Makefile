#!/usr/bin/make -f
# Makefile for G2B Brein (DISTRHO Plugin Framework)

NAME = g2b_brein_gio

FILES_DSP = G2BBreinPlugin.cpp vendor/MidiFile.cpp vendor/MidiMessage.cpp vendor/MidiEvent.cpp vendor/MidiEventList.cpp vendor/Binasc.cpp vendor/Options.cpp
FILES_UI  = G2BBreinUI.cpp

include ../../Makefile.plugins.mk

TARGETS += vst3
TARGETS += clap

all: $(TARGETS)
