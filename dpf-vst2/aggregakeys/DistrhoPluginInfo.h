/*
 * Headless DPF-based VST2 build of the real AggregaKeys synth engine.
 * This target intentionally has no custom UI in this phase.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Aggregatron"
#define DISTRHO_PLUGIN_NAME    "AggregaKeys v0.9 DPF VST2"
#define DISTRHO_PLUGIN_URI     "urn:aggregatron:aggregakeys:dpf-vst2"

#define DISTRHO_PLUGIN_BRAND_ID  Agtr
#define DISTRHO_PLUGIN_UNIQUE_ID Akd2

#define DISTRHO_PLUGIN_HAS_UI          0
#define DISTRHO_PLUGIN_IS_RT_SAFE      1
#define DISTRHO_PLUGIN_IS_SYNTH        1
#define DISTRHO_PLUGIN_NUM_INPUTS      0
#define DISTRHO_PLUGIN_NUM_OUTPUTS     2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 1

enum AggregaKeysDPFParameters {
    kParameterGain = 0,
    kParameterVelocityAmp,
    kParameterAttack,
    kParameterDecay,
    kParameterSustain,
    kParameterRelease,
    kParameterOsc1Wave,
    kParameterOsc2Wave,
    kParameterOscMix,
    kParameterOsc2Detune,
    kParameterOsc2Fine,
    kParameterFilterCutoff,
    kParameterFilterResonance,
    kParameterFilterEnvAmount,
    kParameterVelocityFilter,
    kParameterFilterAttack,
    kParameterFilterDecay,
    kParameterFilterSustain,
    kParameterFilterRelease,
    kParameterLfoRate,
    kParameterLfoPitchDepth,
    kParameterLfoFilterDepth,
    kParameterGlide,
    kParameterPolyphony,
    kParameterMonoMode,
    kParameterDrive,
    kParameterReverbMix,
    kParameterReverbSize,
    kParameterReverbDamping,
    kParameterCount
};

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
