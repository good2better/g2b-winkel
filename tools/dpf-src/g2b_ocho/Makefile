#!/usr/bin/make -f
# Makefile for G2B OchoOcho (DISTRHO Plugin Framework)

NAME = g2b_ocho

FILES_DSP = OchoPlugin.cpp
FILES_UI  = OchoUI.cpp

include ../../Makefile.plugins.mk

# SHGetKnownFolderPath (echte Documenten/Muziek-locatie) + FOLDERID guids
ifeq ($(WINDOWS),true)
EXTRA_LIBS += -lshell32 -luuid -lole32
endif

TARGETS += vst3
TARGETS += clap
TARGETS += jack

all: $(TARGETS)
