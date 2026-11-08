/*
 * G2B gedeelde bus — sporen praten via shared memory.
 * MIXER-instanties schrijven hun 8-band energie + PRIO in slots.
 * MASTER schrijft zijn whole-mix band-energie + een top-down mask
 * (welke band overvol is → lage-prio sporen duken extra daar).
 */
#pragma once
#include <cstdint>
#include <cstring>
#if defined(DISTRHO_OS_WINDOWS)
# include <windows.h>
#else
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

static const int kBusSlots=32, kBusBands=8;
struct G2BBusSlot { volatile uint32_t id; volatile uint32_t pad; volatile uint64_t tick; volatile uint32_t prio; volatile uint32_t pad2; volatile float e[kBusBands]; };
struct G2BBus {
    volatile uint32_t magic; uint32_t pad0;
    G2BBusSlot s[kBusSlots];              // 32 × 56B
    volatile uint64_t masterTick;         // MASTER leeft als dit oploopt
    volatile float masterE[kBusBands];    // band-energie van de hele mix
    volatile float masterMask[kBusBands]; // 0..1: extra dip voor lage prio
    // MIDI-handoff: GELUID->MIDI publiceert hier zijn antwoord-patroon;
    // BAND leest het als melodie-seed voor het arrangement. Aan het eind
    // van de struct geplakt zodat oudere plugins alle offsets behouden.
    volatile uint32_t midiMagic;          // kMidiMagic
    volatile uint64_t midiSeq;            // oploopt bij elke publicatie
    volatile uint32_t midiGenre;          // genre-pill index van de bron
    volatile uint32_t midiKeyMin;         // root 0-11, +12 bij mineur
    volatile uint32_t midiCount;          // aantal noten in midiNote
    volatile uint16_t midiAt16[256];      // positie in 16e-noten
    volatile uint8_t  midiNote[256];
    volatile uint8_t  midiVel[256];
};
static const uint32_t kMidiMagic = 0x4732424Du; // 'G2BM'
static const uint32_t kBusMagic = 0x47324232u; // 'G2B2'

static void* g2bBusOpen() {
#if defined(DISTRHO_OS_WINDOWS)
    HANDLE h=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(G2BBus),"Local\\G2BMixerBus");
    if(!h) return nullptr;
    void* p=MapViewOfFile(h,FILE_MAP_ALL_ACCESS,0,0,sizeof(G2BBus));
    return p;
#else
    int fd=shm_open("/g2b_mixer_bus",O_RDWR|O_CREAT,0600);
    if(fd<0) return nullptr;
    struct stat st; if(fstat(fd,&st)==0 && st.st_size<(off_t)sizeof(G2BBus)) ftruncate(fd,sizeof(G2BBus));
    void* p=mmap(nullptr,sizeof(G2BBus),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    close(fd);
    if(p==MAP_FAILED) return nullptr;
    return p;
#endif
}
static void g2bBusClose(void* p) {
    if(!p) return;
#if defined(DISTRHO_OS_WINDOWS)
    UnmapViewOfFile(p);
#else
    munmap(p,sizeof(G2BBus));
#endif
}
