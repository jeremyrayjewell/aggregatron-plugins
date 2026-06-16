/*
 * Temporary DPF-based VST2 probe for validating the Windows x64 toolchain.
 * This file does not contain AggregaKeys DSP.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Aggregatron"
#define DISTRHO_PLUGIN_NAME    "AggregaKeys VST2 Probe"
#define DISTRHO_PLUGIN_URI     "urn:aggregatron:aggregakeys:vst2-probe"

#define DISTRHO_PLUGIN_BRAND_ID  Agtr
#define DISTRHO_PLUGIN_UNIQUE_ID Akp2

#define DISTRHO_PLUGIN_HAS_UI          0
#define DISTRHO_PLUGIN_IS_RT_SAFE      1
#define DISTRHO_PLUGIN_IS_SYNTH        1
#define DISTRHO_PLUGIN_NUM_INPUTS      0
#define DISTRHO_PLUGIN_NUM_OUTPUTS     2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 1

enum Parameters {
    kParameterGain = 0,
    kParameterCount
};

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
