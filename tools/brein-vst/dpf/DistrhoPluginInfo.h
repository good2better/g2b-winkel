/*
 * G2B Brein — patroon-brein MIDI-generator (VSTi → MIDI-out)
 * Plugin info for DISTRHO Plugin Framework.
 */

#ifndef DISTRHO_PLUGIN_INFO_H
#define DISTRHO_PLUGIN_INFO_H

#define DISTRHO_PLUGIN_BRAND   "G2B"
#define DISTRHO_PLUGIN_NAME    "G2B Brein"
#define DISTRHO_PLUGIN_URI     "https://github.com/good2better/g2b-brein"
#define DISTRHO_PLUGIN_CLAP_ID "nl.g2b.brein"

#define DISTRHO_PLUGIN_BRAND_ID  G2Bt
#define DISTRHO_PLUGIN_UNIQUE_ID G2Br

#define DISTRHO_PLUGIN_IS_SYNTH      1
#define DISTRHO_PLUGIN_HAS_UI        1
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_NUM_INPUTS    0
#define DISTRHO_PLUGIN_NUM_OUTPUTS   2
#define DISTRHO_PLUGIN_WANT_PROGRAMS 0
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT 1
#define DISTRHO_PLUGIN_WANT_TIMEPOS   1
#define DISTRHO_PLUGIN_WANT_STATE    1
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Instrument|Generator"
#define DISTRHO_PLUGIN_CLAP_FEATURES "instrument", "note-generator", "stereo"
#define DISTRHO_UI_FILE_BROWSER      0
#define DISTRHO_UI_USER_RESIZABLE    1
#define DISTRHO_UI_USE_NANOVG        1
#define DISTRHO_UI_DEFAULT_WIDTH     640
#define DISTRHO_UI_DEFAULT_HEIGHT    430

#endif // DISTRHO_PLUGIN_INFO_H
