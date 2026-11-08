#!/usr/bin/make -f
NAME = g2b_oproep_gio
FILES_DSP = OproepPlugin.cpp vendor/MidiFile.cpp vendor/MidiMessage.cpp vendor/MidiEvent.cpp vendor/MidiEventList.cpp vendor/Binasc.cpp vendor/Options.cpp
FILES_UI  = OproepUI.cpp DragOut.cpp
include ../../Makefile.plugins.mk
TARGETS += vst3
TARGETS += clap
TARGETS += jack
all: $(TARGETS)
ifeq ($(WINDOWS),true)
EXTRA_LIBS += -lshell32 -luuid -lole32 -lshlwapi
endif
