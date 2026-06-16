# AggregaKeys VST2 Port Audit

Audit date: 2026-06-16  
Repository: `aggregatron-plugins`  
Scope: `AggregaKeys` only  
Change policy for this phase: no code/build/dependency changes; report only

## 1. Exact files that implement AggregaKeys

### Main source files

- `src/PluginProcessor.h:1-64`
  - Declares `AggregatronKeysAudioProcessor`
  - Declares audio/MIDI/state entry points
  - Owns `juce::AudioProcessorValueTreeState`, `juce::MidiKeyboardState`, buffers, smoothed gains, waveform snapshot state

- `src/PluginProcessor.cpp:1-885`
  - Contains the entire AggregaKeys audio engine in one translation unit
  - Defines oscillator helpers, `SynthSound`, `SynthVoice`, `AggregaKeysSynth`, and all `AggregatronKeysAudioProcessor` methods

- `src/PluginEditor.h:1-136`
  - Declares `AggregatronKeysAudioProcessorEditor`
  - Declares UI controls, attachments, QWERTY state maps, timer/key handling

- `src/PluginEditor.cpp:1-1092`
  - Implements the complete JUCE editor
  - Implements embedded resource loading, parameter widgets, preset menu, QWERTY note entry, on-screen keyboard, and layout

### Component-by-component mapping

#### AudioProcessor

- Class: `AggregatronKeysAudioProcessor`
- Declaration: `src/PluginProcessor.h:4-64`
- Implementation: `src/PluginProcessor.cpp:561-885`

Relevant methods:

- Constructor: `src/PluginProcessor.cpp:561-571`
- `prepareToPlay`: `src/PluginProcessor.cpp:573-586`
- `processBlock`: `src/PluginProcessor.cpp:600-677`
- `getStateInformation`: `src/PluginProcessor.cpp:739-743`
- `setStateInformation`: `src/PluginProcessor.cpp:745-750`
- `savePresetToFile`: `src/PluginProcessor.cpp:763-769`
- `loadPresetFromFile`: `src/PluginProcessor.cpp:771-785`

#### Editor / UI

- Class: `AggregatronKeysAudioProcessorEditor`
- Declaration: `src/PluginEditor.h:12-136`
- Implementation: `src/PluginEditor.cpp:350-1092`

Relevant methods:

- Constructor: `src/PluginEditor.cpp:350-532`
- `paint`: `src/PluginEditor.cpp:942-970`
- `resized`: `src/PluginEditor.cpp:972-1092`
- `applyFactoryPreset`: `src/PluginEditor.cpp:801-923`

#### Synth engine

- Class: `AggregaKeysSynth`
- Implementation only: `src/PluginProcessor.cpp:373-558`
- This is a JUCE `juce::Synthesiser` subclass
- Handles mono/poly policy, held-note tracking, voice selection, mono legato note routing

#### Voices and sounds

- `SynthSound`: `src/PluginProcessor.cpp:73-78`
- `SynthVoice`: `src/PluginProcessor.cpp:80-371`

`SynthVoice` contains:

- oscillator state
- envelopes
- filter
- modulation
- glide smoothing
- note rendering

#### Oscillators

There are no separate oscillator source files. Oscillator generation is implemented as free functions plus voice-local phase state:

- `noteNumberToFrequency`: `src/PluginProcessor.cpp:6-9`
- `wrapPhase01`: `src/PluginProcessor.cpp:11-15`
- `wrapPhaseRadians`: `src/PluginProcessor.cpp:17-26`
- `polyBlep`: `src/PluginProcessor.cpp:28-46`
- `sawWave`: `src/PluginProcessor.cpp:48-53`
- `squareWave`: `src/PluginProcessor.cpp:55-61`
- `triangleWave`: `src/PluginProcessor.cpp:63-66`
- `sineWave`: `src/PluginProcessor.cpp:68-71`
- waveform selection in `SynthVoice::renderOscillatorSample`: `src/PluginProcessor.cpp:317-331`
- oscillator stepping/render use in `SynthVoice::renderNextBlock`: `src/PluginProcessor.cpp:183-200`

#### Envelopes

- Amp envelope: `juce::ADSR ampEnvelope`
- Filter envelope: `juce::ADSR filterEnvelope`
- Members declared at `src/PluginProcessor.cpp:334-335`

Relevant code:

- sample-rate setup/reset: `src/PluginProcessor.cpp:102-121`
- note-on trigger: `src/PluginProcessor.cpp:128-149`
- note-off trigger: `src/PluginProcessor.cpp:151-158`
- parameter updates: `src/PluginProcessor.cpp:254-268`

#### Filters

- Filter type: `juce::dsp::StateVariableTPTFilter<float>`
- Member declaration: `src/PluginProcessor.cpp:336`

Relevant code:

- set lowpass type in constructor: `src/PluginProcessor.cpp:91-95`
- prepare/reset on sample-rate change: `src/PluginProcessor.cpp:114-120`
- per-sample cutoff/resonance updates: `src/PluginProcessor.cpp:202-209`

#### MIDI handling

Processor-side:

- `juce::MidiKeyboardState keyboardState` declared in `src/PluginProcessor.h:38,55`
- `keyboardState.processNextMidiBuffer(...)`: `src/PluginProcessor.cpp:609`
- incoming MIDI iteration: `src/PluginProcessor.cpp:611-625`
- pitch wheel injection from UI state: `src/PluginProcessor.cpp:627-632`
- mod wheel injection from UI state: `src/PluginProcessor.cpp:634-639`
- synth render from `MidiBuffer`: `src/PluginProcessor.cpp:644`

Synth-side:

- `AggregaKeysSynth::noteOn`: `src/PluginProcessor.cpp:395-415`
- `AggregaKeysSynth::noteOff`: `src/PluginProcessor.cpp:417-443`
- `AggregaKeysSynth::findFreeVoice`: `src/PluginProcessor.cpp:445-492`
- `SynthVoice::pitchWheelMoved`: `src/PluginProcessor.cpp:160-164`
- `SynthVoice::controllerMoved`: `src/PluginProcessor.cpp:166-170`

Editor-side UI/QWERTY:

- `keyPressed`: `src/PluginEditor.cpp:675-710`
- `releaseStaleHeldKeys`: `src/PluginEditor.cpp:722-755`
- `releaseComputerKeyboardNotes`: `src/PluginEditor.cpp:757-769`
- `processPendingComputerKeyboardNoteOffs`: `src/PluginEditor.cpp:771-799`
- `focusLost`: `src/PluginEditor.cpp:669-673`
- `timerCallback`: `src/PluginEditor.cpp:925-940`

#### Parameter declarations

- `AggregatronKeysAudioProcessor::createParameterLayout`: `src/PluginProcessor.cpp:843-880`

#### Parameter-to-DSP synchronization

- Voice-local parameter pulls from APVTS atomics: `src/PluginProcessor.cpp:254-300`
- voice-count/mono sync: `src/PluginProcessor.cpp:798-810`
- reverb sync: `src/PluginProcessor.cpp:812-821`
- output gain/reverb mix smoothing targets: `src/PluginProcessor.cpp:581-585`, `646-647`

#### State serialization

- plugin state chunk: `src/PluginProcessor.cpp:739-750`
- file preset save/load: `src/PluginProcessor.cpp:763-785`

#### Presets

Factory presets:

- Implemented in editor only: `src/PluginEditor.cpp:801-923`
- Names: `Init`, `Soft Pad`, `Bass Punch`, `Glass Mono`, `Wide Lead`
- Menu declaration: `src/PluginEditor.cpp:422-434`

User file presets:

- Save button/file chooser: `src/PluginEditor.cpp:438-448`
- Load button/file chooser: `src/PluginEditor.cpp:450-460`
- Backing persistence in processor: `src/PluginProcessor.cpp:763-785`

#### Embedded resources

Bundled in CMake:

- `assets/bg.png`
- `assets/knob.png`
- `assets/slider.png`
- `assets/font.ttf`

Declaration:

- `CMakeLists.txt:17-24`

Usage in editor constructor:

- `backgroundImage`: `src/PluginEditor.cpp:353`
- `knobImage`: `src/PluginEditor.cpp:354`
- `sliderImage`: `src/PluginEditor.cpp:355`
- `titleFont` and `bodyFont` from binary font data: `src/PluginEditor.cpp:356-357`

## 2. Current AggregaKeys CMake targets and source lists

### Shared logical target

- `juce_add_plugin(AggregatronKeys ...)`: `CMakeLists.txt:27-40`

Current format list:

- `FORMATS Standalone LV2 VST3`: `CMakeLists.txt:38`

Current AggregaKeys metadata in CMake:

- `COMPANY_NAME "Aggregatron"`: `CMakeLists.txt:28`
- `BUNDLE_ID "com.aggregatron.aggregakeys.v0_2"`: `CMakeLists.txt:29`
- `LV2URI "urn:aggregatron:aggregakeys"`: `CMakeLists.txt:30`
- `PRODUCT_NAME "AggregaKeys v0.9"`: `CMakeLists.txt:39`

### Source list

- `src/PluginEditor.cpp`: `CMakeLists.txt:45`
- `src/PluginEditor.h`: `CMakeLists.txt:46`
- `src/PluginProcessor.cpp`: `CMakeLists.txt:47`
- `src/PluginProcessor.h`: `CMakeLists.txt:48`

### Linked JUCE modules for AggregaKeys

- `juce::juce_audio_basics`: `CMakeLists.txt:64`
- `juce::juce_audio_devices`: `CMakeLists.txt:65`
- `juce::juce_audio_formats`: `CMakeLists.txt:66`
- `juce::juce_audio_plugin_client`: `CMakeLists.txt:67`
- `juce::juce_audio_processors`: `CMakeLists.txt:68`
- `juce::juce_audio_utils`: `CMakeLists.txt:69`
- `juce::juce_core`: `CMakeLists.txt:70`
- `juce::juce_data_structures`: `CMakeLists.txt:71`
- `juce::juce_dsp`: `CMakeLists.txt:72`
- `juce::juce_events`: `CMakeLists.txt:73`
- `juce::juce_graphics`: `CMakeLists.txt:74`
- `juce::juce_gui_basics`: `CMakeLists.txt:75`
- `juce::juce_gui_extra`: `CMakeLists.txt:76`

### Generated Windows build targets currently present

Observed in `build/AggregatronKeys_All.vcxproj`:

- `AggregatronKeys_LV2`: `build/AggregatronKeys_All.vcxproj`
- `AggregatronKeys_Standalone`: `build/AggregatronKeys_All.vcxproj`
- `AggregatronKeys_VST3`: `build/AggregatronKeys_All.vcxproj`

## 3. JUCE classes/modules used by the audio-rendering path

This section separates current dependencies into functional groups. It distinguishes between:

- classes directly referenced in AggregaKeys source
- JUCE modules linked by the target even when AggregaKeys source does not reference them explicitly

### Plugin-wrapper dependencies

Direct classes/interfaces in source:

- `juce::AudioProcessor`: `src/PluginProcessor.h:4`
- `juce::AudioProcessorEditor`: `src/PluginEditor.h:12`
- `juce::AudioProcessorValueTreeState`: `src/PluginProcessor.h:37`, `src/PluginEditor.h:31-32`
- `juce::Synthesiser`: `src/PluginProcessor.cpp:373`
- `juce::SynthesiserVoice`: `src/PluginProcessor.cpp:80`
- `juce::SynthesiserSound`: `src/PluginProcessor.cpp:73`

Linked module targets:

- `juce::juce_audio_plugin_client`: wrapper/export layer for Standalone/LV2/VST3
- `juce::juce_audio_processors`: `AudioProcessor`, `AudioProcessorValueTreeState`, `Synthesiser`, `MidiKeyboardState`
- `juce::juce_audio_devices`: needed by the Standalone wrapper target

### GUI dependencies

Direct classes in source:

- `juce::AudioProcessorEditor`
- `juce::Label`
- `juce::Slider`
- `juce::ComboBox`
- `juce::TextButton`
- `juce::MidiKeyboardComponent`
- `juce::LookAndFeel_V4`
- `juce::Graphics`
- `juce::Path`
- `juce::Image`
- `juce::ImageCache`
- `juce::Typeface`
- `juce::Font`
- `juce::FileChooser`
- `juce::Component`
- `juce::Timer`
- `juce::KeyListener`

Linked module targets:

- `juce::juce_gui_basics`
- `juce::juce_gui_extra`
- `juce::juce_graphics`
- `juce::juce_events`
- `juce::juce_audio_utils`

### DSP or MIDI dependencies

Direct classes in source:

- `juce::AudioBuffer<float>`: `src/PluginProcessor.h:57-58`, `src/PluginProcessor.cpp:172`, `600`
- `juce::MidiBuffer`: `src/PluginProcessor.h:17`, `src/PluginProcessor.cpp:600`
- `juce::MidiMessage`: `src/PluginEditor.cpp:37`, `src/PluginProcessor.cpp:630,637`
- `juce::MidiKeyboardState`: `src/PluginProcessor.h:38,55`
- `juce::ADSR`: `src/PluginProcessor.cpp:334-335`
- `juce::dsp::StateVariableTPTFilter<float>`: `src/PluginProcessor.cpp:336`
- `juce::dsp::ProcessSpec`: `src/PluginProcessor.cpp:114-118`
- `juce::SmoothedValue<float>`: `src/PluginProcessor.cpp:360-362`, `src/PluginProcessor.h:59-60`
- `juce::Reverb`: `src/PluginProcessor.h:56`
- `juce::ScopedNoDenormals`: `src/PluginProcessor.cpp:602`

Linked module targets:

- `juce::juce_audio_basics`
- `juce::juce_audio_processors`
- `juce::juce_dsp`
- `juce::juce_audio_utils`

### Utility dependencies

Direct classes/utilities in source:

- `juce::CriticalSection` and `juce::ScopedLock`: `src/PluginProcessor.h:61`, `src/PluginProcessor.cpp:789,839`
- `juce::XmlDocument`: `src/PluginProcessor.cpp:776`
- `juce::ValueTree`: `src/PluginProcessor.cpp:749,783`
- `juce::MemoryBlock`: `src/PluginProcessor.h:34-35`
- `juce::File`: `src/PluginProcessor.cpp:763,771`, `src/PluginEditor.cpp:440,452`
- `juce::Time`: `src/PluginEditor.cpp:707,737,776`
- `juce::CharacterFunctions`: `src/PluginEditor.cpp:23`

Linked module targets:

- `juce::juce_core`
- `juce::juce_data_structures`

### Important observation about module minimization

The current target links many GUI modules into the same AggregaKeys target, but the actual audio-rendering path in `src/PluginProcessor.cpp` directly depends only on a subset of those modules. A DPF wrapper could likely avoid most GUI modules if it does not reuse the JUCE editor in phase 1.

## 4. Is the synth engine separable from AudioProcessor?

### Current state

Partially separable, but not currently extracted.

Evidence:

- `SynthVoice`, `SynthSound`, and `AggregaKeysSynth` are already distinct classes: `src/PluginProcessor.cpp:73-558`
- The processor mostly orchestrates:
  - host/keyboard MIDI ingestion
  - voice-count and mono-mode sync
  - reverb/output mixing
  - state/preset serialization

### Why it is not yet framework-neutral

The current engine is still tightly bound to JUCE:

- voices subclass `juce::SynthesiserVoice`
- the synth subclasses `juce::Synthesiser`
- parameters are read directly from `juce::AudioProcessorValueTreeState` atomics inside `SynthVoice::updateParameters`: `src/PluginProcessor.cpp:254-300`
- processor owns buffers and final wet/dry mix path: `src/PluginProcessor.cpp:641-675`
- processor owns reverb and output gain smoothing: `src/PluginProcessor.h:56,59-60`, `src/PluginProcessor.cpp:581-585,646-647`

### Audit conclusion

- DSP logic is not embedded directly in `processBlock()` alone
- but it is embedded directly in `src/PluginProcessor.cpp`
- and it is not yet isolated behind a framework-neutral engine API

## 5. Complete AggregaKeys parameter inventory

Declared in `src/PluginProcessor.cpp:848-877`.

| ID | Visible name | Type | Range / choices | Default | Skew / step | Audio-thread read path |
|---|---|---|---|---|---|---|
| `gain` | Gain | `AudioParameterFloat` | `0.0..1.0` | `0.7` | default | `processBlock()` via `getRawParameterValue("gain")->load()` at `583,646` |
| `velocityAmp` | Velocity Amp | `AudioParameterFloat` | `0.0..1.0` | `0.0` | default | `SynthVoice::updateParameters()` `283` |
| `attack` | Attack | `AudioParameterFloat` | `0.001..3.0` | `0.02` | step `0.001`, skew `0.4` | `257` |
| `decay` | Decay | `AudioParameterFloat` | `0.001..3.0` | `0.15` | step `0.001`, skew `0.4` | `258` |
| `sustain` | Sustain | `AudioParameterFloat` | `0.0..1.0` | `0.8` | default | `259` |
| `release` | Release | `AudioParameterFloat` | `0.001..5.0` | `0.35` | step `0.001`, skew `0.4` | `260` |
| `osc1Wave` | Osc 1 Wave | `AudioParameterChoice` | `Saw, Square, Triangle, Sine` | index `0` | discrete choice | `270` |
| `osc2Wave` | Osc 2 Wave | `AudioParameterChoice` | `Saw, Square, Triangle, Sine` | index `1` | discrete choice | `271` |
| `oscMix` | Osc Mix | `AudioParameterFloat` | `0.0..1.0` | `0.35` | default | `272` |
| `osc2Detune` | Osc 2 Detune | `AudioParameterFloat` | `-12.0..12.0` | `0.0` | default | `273` |
| `osc2Fine` | Osc 2 Fine | `AudioParameterFloat` | `-50.0..50.0` | `0.0` | default | `274` |
| `filterCutoff` | Filter Cutoff | `AudioParameterFloat` | `40.0..18000.0` | `4000.0` | step `1.0`, skew `0.25` | `275` |
| `filterResonance` | Filter Resonance | `AudioParameterFloat` | `0.1..1.2` | `0.3` | default | `276` |
| `filterEnvAmount` | Filter Env Amount | `AudioParameterFloat` | `0.0..12000.0` | `2500.0` | default | `277` |
| `velocityFilter` | Velocity Filter | `AudioParameterFloat` | `0.0..6000.0` | `0.0` | default | `278` |
| `filterAttack` | Filter Attack | `AudioParameterFloat` | `0.001..3.0` | `0.01` | step `0.001`, skew `0.4` | `264` |
| `filterDecay` | Filter Decay | `AudioParameterFloat` | `0.001..3.0` | `0.2` | step `0.001`, skew `0.4` | `265` |
| `filterSustain` | Filter Sustain | `AudioParameterFloat` | `0.0..1.0` | `0.1` | default | `266` |
| `filterRelease` | Filter Release | `AudioParameterFloat` | `0.001..5.0` | `0.3` | step `0.001`, skew `0.4` | `267` |
| `lfoRate` | LFO Rate | `AudioParameterFloat` | `0.05..20.0` | `5.0` | step `0.01`, skew `0.3` | `279` |
| `lfoPitchDepth` | LFO Pitch Depth | `AudioParameterFloat` | `0.0..1.0` | `0.0` | default | `280` |
| `lfoFilterDepth` | LFO Filter Depth | `AudioParameterFloat` | `0.0..6000.0` | `0.0` | default | `281` |
| `glide` | Glide | `AudioParameterFloat` | `0.0..1.5` | `0.0` | step `0.001`, skew `0.35` | `282` |
| `polyphony` | Polyphony | `AudioParameterInt` | integer `1..16` | `8` | integer step `1` | `806` |
| `monoMode` | Mono Mode | `AudioParameterBool` | `false/true` | `false` | boolean | `807` |
| `drive` | Drive | `AudioParameterFloat` | `0.0..1.0` | `0.15` | default | `284`, remapped to `1.0..8.0` |
| `reverbMix` | Reverb Mix | `AudioParameterFloat` | `0.0..1.0` | `0.18` | default | `584,647` |
| `reverbSize` | Reverb Size | `AudioParameterFloat` | `0.0..1.0` | `0.45` | default | `815` |
| `reverbDamping` | Reverb Damping | `AudioParameterFloat` | `0.0..1.0` | `0.3` | default | `816` |

### Automation safety

What is known from the code:

- all runtime parameter reads use `parameters.getRawParameterValue(...)->load()`
- this happens on the audio thread in `processBlock()`, `updateParameters()`, `syncVoiceCount()`, and `updateEffectParameters()`
- no locks are taken around parameter loads

Audit conclusion:

- parameter value reads appear designed for audio-thread polling through APVTS atomics
- this is the primary compatibility contract a DPF adapter would need to preserve
- exact JUCE internal memory-order semantics are not restated in repository code, so anything beyond “lock-free-style polling via APVTS raw values” is unknown from this repo alone

## 6. MIDI requirements

### Note on

- Required
- Handled by `AggregaKeysSynth::noteOn`: `src/PluginProcessor.cpp:395-415`
- External MIDI path enters through `synth->renderNextBlock(..., midiMessages, ...)`: `src/PluginProcessor.cpp:644`
- QWERTY path injects `keyboardState.noteOn(1, noteNumber, 0.9f)`: `src/PluginEditor.cpp:706-708`

### Note off

- Required
- Handled by `AggregaKeysSynth::noteOff`: `src/PluginProcessor.cpp:417-443`
- QWERTY path injects deferred/immediate `keyboardState.noteOff(1, ...)`: `src/PluginEditor.cpp:699-703,741-750,757-763,790`

### Velocity

- Required and used
- Stored per voice in `noteVelocity`: `src/PluginProcessor.cpp:136`
- Impacts output gain and filter cutoff modulation: `src/PluginProcessor.cpp:203,211-212`

### Pitch bend

- Required and used
- Voice-side callback: `src/PluginProcessor.cpp:160-164`
- UI pitch wheel writes processor atomic: `src/PluginEditor.cpp:505-514`
- Processor injects `MidiMessage::pitchWheel(1, pitchWheel)` at block offset 0: `src/PluginProcessor.cpp:627-632`

### Modulation wheel

- Required and used
- Voice-side callback listens for controller `1`: `src/PluginProcessor.cpp:166-170`
- Used as vibrato contribution: `src/PluginProcessor.cpp:185-187`
- UI mod wheel writes processor atomic: `src/PluginEditor.cpp:516-520`
- Processor injects controller event on channel 1 at block offset 0: `src/PluginProcessor.cpp:634-639`

### Sustain

- No explicit custom sustain implementation is present in AggregaKeys source
- `findFreeVoice` checks `isSustainPedalDown()` and `isSostenutoPedalDown()` when reusing voices: `src/PluginProcessor.cpp:467-470`

Audit conclusion:

- sustain behavior appears to rely on inherited JUCE synthesiser/controller handling rather than custom AggregaKeys code
- exact sustain-controller message handling is not implemented explicitly in repository source

### All-notes-off

- Editor panic path sends `audioProcessor.getKeyboardState().allNotesOff(1)`: `src/PluginEditor.cpp:683-690`
- No processor-side custom all-notes-off implementation is present

Audit conclusion:

- UI panic exists for channel 1 QWERTY/on-screen use
- host-side all-notes-off handling likely depends on inherited JUCE synth behavior
- explicit repository-local implementation is otherwise absent

### Channel handling

- `SynthSound` applies to all channels: `src/PluginProcessor.cpp:76-77`
- `AggregaKeysSynth` note tracking stores `midiChannel` in `HeldNote`: `src/PluginProcessor.cpp:495-500`
- external MIDI note events preserve incoming channel into `noteOn`/`noteOff`
- UI-injected pitch/mod events are forced to channel 1: `src/PluginProcessor.cpp:630,637`
- QWERTY note events and panic use channel 1: `src/PluginEditor.cpp:689,701,708,749,760,763,790`

### Sample-offset accuracy within each audio block

What the code shows:

- `juce::MidiBuffer` events are passed directly to `synth->renderNextBlock(...)`: `src/PluginProcessor.cpp:644`
- external MIDI metadata is iterated with its original sample offsets: `src/PluginProcessor.cpp:611-625`
- processor-inserted UI pitch/mod events are always inserted at sample offset `0`: `src/PluginProcessor.cpp:630,637`
- `keyboardState.processNextMidiBuffer(...)` is called for the entire current block: `src/PluginProcessor.cpp:609`

Audit conclusion:

- external host MIDI can be sample-offset aware within the block because JUCE `MidiBuffer` offsets are preserved to the synth
- UI-generated pitch/mod events are block-start only
- QWERTY release timing is timer/millisecond based in the editor, so its retrigger and deferred note-off path is not sample-accurate in the audio sense
- exact sub-block timing details of `MidiKeyboardState` generated events are not independently verifiable from this repository alone

## 7. Audio configuration

### Inputs

- No audio input bus is declared
- Constructor creates outputs only: `src/PluginProcessor.cpp:561-563`

Audit conclusion:

- inputs: `0`

### Outputs

- Main output bus is stereo in constructor: `src/PluginProcessor.cpp:562`
- `isBusesLayoutSupported` accepts mono or stereo output layouts: `src/PluginProcessor.cpp:593-597`

Audit conclusion:

- supported outputs: mono or stereo
- internal render path is stereo-capable, with mono fallback branches

### Mono/stereo assumptions

- voice rendering writes left and optional right channels: `src/PluginProcessor.cpp:180-181,214-216`
- reverb handles mono or stereo separately: `src/PluginProcessor.cpp:652-659`

### Sample-rate handling

- processor stores `currentSampleRate`: `src/PluginProcessor.cpp:575`
- synth sample rate is pushed in `prepareToPlay`: `src/PluginProcessor.cpp:576`
- voices update ADSR/filter preparation in `setCurrentPlaybackSampleRate`: `src/PluginProcessor.cpp:102-121`
- smoothed parameters are reset from sample rate: `src/PluginProcessor.cpp:244-251,581-582`

### Block-size handling

- `prepareToPlay` sizes `synthBuffer` and `wetBuffer` to `samplesPerBlock`: `src/PluginProcessor.cpp:578-579`
- `processBlock` resizes both buffers again to current block size: `src/PluginProcessor.cpp:641-642`

### Tail length

- Explicit tail length: `2.0` seconds: `src/PluginProcessor.cpp:708-711`

### Latency

- No explicit latency reporting or `setLatencySamples(...)` call is present

Audit conclusion:

- explicit latency value in repository code: none
- effective intended latency appears to be `0`, but that is inferred rather than declared

### Realtime allocations or locks

Confirmed in current code:

- `synthBuffer.setSize(...)` and `wetBuffer.setSize(...)` are called every block: `src/PluginProcessor.cpp:641-642`
- `captureWaveformSnapshot` allocates `std::vector<float> snapshot(192, ...)` every block: `src/PluginProcessor.cpp:829-837`
- `captureWaveformSnapshot` takes a `juce::ScopedLock` on `waveformLock`: `src/PluginProcessor.cpp:839`
- `getWaveformSnapshot` also locks: `src/PluginProcessor.cpp:787-795`

Audit conclusion:

- the current render path is not strictly allocation-free
- the current render path is not strictly lock-free
- those behaviors are acceptable in the current JUCE app/plugin, but are notable for a future cross-wrapper core

## 8. State and preset compatibility requirements for a future DPF VST2 build

Current state format:

- plugin chunk state is the APVTS `ValueTree` serialized to XML and then packed via `copyXmlToBinary`: `src/PluginProcessor.cpp:739-743`
- restore expects the same XML tag as `parameters.state.getType()`: `src/PluginProcessor.cpp:745-750`

Current preset file format:

- `savePresetToFile` writes the APVTS XML directly to disk: `src/PluginProcessor.cpp:763-769`
- `loadPresetFromFile` parses XML and replaces the APVTS state: `src/PluginProcessor.cpp:771-785`

Compatibility requirements if a DPF build must interoperate with current JUCE formats:

1. Preserve all parameter IDs exactly as declared in `createParameterLayout`.
2. Preserve all parameter semantic ranges/defaults/choice ordering.
3. Preserve the meaning of `monoMode` and `polyphony` exactly, because mono/poly routing changes behavior in `syncVoiceCount` and `AggregaKeysSynth`.
4. Preserve the APVTS-like state schema or provide a deterministic import/export translation layer.
5. Preserve factory preset values from `applyFactoryPreset`.
6. Preserve file preset XML compatibility if users are expected to share presets between JUCE and future DPF builds.

Biggest compatibility challenge:

- current state and preset logic are built directly on JUCE `AudioProcessorValueTreeState`
- a DPF VST2 build would need either:
  - the same XML/value-tree schema reimplemented or reused
  - or an explicit adapter/translator

## 9. Comparison of two implementation approaches

### Approach A

DPF VST2 wrapper that reuses the current AggregaKeys engine and links only the minimum necessary JUCE modules.

#### Practical reading of “reuse current engine”

In this repo, “current engine” is not a separate library. It is the set of JUCE-dependent classes inside `src/PluginProcessor.cpp:73-558`, plus processor-owned wet/dry mix/state logic in `src/PluginProcessor.cpp:561-841`.

#### Files likely affected

Estimated:

- `CMakeLists.txt`
- `src/PluginProcessor.cpp`
- `src/PluginProcessor.h`
- 2 to 5 new DPF-side wrapper/build files
- possibly 1 to 2 new shared helper files if some code is split out of `PluginProcessor.cpp`

Estimated total: about 5 to 9 files

#### Architectural risk

- Medium

Reasons:

- less invasive than a full framework-neutral extraction
- but current engine still depends directly on JUCE synth classes and APVTS
- DPF cannot directly instantiate a JUCE `AudioProcessor` as a VST2 wrapper substitute without adaptation work

#### Likely code duplication

- Low to medium

Likely duplication areas:

- parameter declaration metadata
- state serialization bridge
- wrapper-specific MIDI/event translation

#### GUI difficulty

- High for current visual editor
- Low to medium for headless or generic editor

Reason:

- the existing UI is fully JUCE-specific: `src/PluginEditor.h/.cpp`
- recreating this inside DPF would be a separate UI port, not a wrapper toggle

#### Parameter/state compatibility difficulty

- Medium

Reason:

- parameter IDs and defaults are clear and stable
- but the current source of truth is APVTS, so DPF-side state must mimic or translate the JUCE XML/value schema

#### Probable time to reach a headless VST2 instrument

- Shortest of the two approaches
- Still non-trivial because there is no existing DPF wrapper and no existing engine abstraction

#### Probable time to reach a VST2 instrument with the current visual editor

- Long
- The current visual editor should be treated as a separate phase

### Approach B

Extract a framework-neutral AggregaKeys core using standard C++ types, with separate JUCE and DPF adapters.

#### Files likely affected

Estimated:

- `src/PluginProcessor.cpp`
- `src/PluginProcessor.h`
- new engine core header/source files
- new parameter schema or engine config files
- JUCE adapter layer files
- DPF adapter layer files
- `CMakeLists.txt`

Estimated total: about 10 to 16 files

#### Architectural risk

- Medium to high initially

Reasons:

- better long-term shape
- but requires moving code out of a currently working JUCE processor/voice stack
- greater risk of regression in mono/poly/glide/QWERTY behavior during extraction

#### Likely code duplication

- Lowest long-term
- Higher temporary scaffolding cost during extraction

#### GUI difficulty

- High for current visual editor
- unchanged relative to Approach A if the goal is the existing JUCE visual editor inside DPF

#### Parameter/state compatibility difficulty

- Medium to high initially

Reasons:

- a framework-neutral core would need a new canonical parameter model
- current JUCE formats would then need adapters to preserve exact IDs/ranges/defaults and preset/state behavior

#### Probable time to reach a headless VST2 instrument

- Longer than Approach A

#### Probable time to reach a VST2 instrument with the current visual editor

- Longest of the two approaches

## 10. Recommended smallest viable first milestone

Recommended milestone:

- A headless or generic-editor DPF VST2 AggregaKeys instrument on Windows
- no attempt to port the current JUCE visual editor in phase 1
- preserve note rendering, mono/poly behavior, glide, pitch bend, mod wheel, and parameter/state identity first

Why this is the smallest viable milestone:

- the current JUCE editor is a separate porting problem
- the synth engine is already conceptually separated into `SynthVoice` and `AggregaKeysSynth`, even though it is not yet extracted into reusable files
- getting sound/MIDI/state working without the JUCE editor is the shortest path to proving VST2 viability

## 11. Proposed staged file-by-file implementation plan

This is a proposal only. No implementation is included in this audit phase.

### Stage 1: create a headless VST2 path with minimal disruption

Expected files to change or be added:

- `CMakeLists.txt`
- `src/PluginProcessor.cpp`
- `src/PluginProcessor.h`
- new DPF wrapper source file(s), likely under a new folder such as `dpf/`
- possibly one new shared AggregaKeys engine file pair if extraction becomes necessary

Suggested work:

1. Introduce DPF as a new dependency/build subtree without removing current JUCE formats.
2. Add a new AggregaKeys-only VST2 target using DPF.
3. Extract the JUCE-dependent synth engine classes from `src/PluginProcessor.cpp` into dedicated files, but keep them JUCE-based in phase 1.
4. Build a DPF plugin wrapper that:
   - exposes the same parameter IDs/names/defaults/ranges
   - translates incoming DPF MIDI/events into the reused engine
   - serializes state in the same XML-compatible form if feasible
5. Omit the current JUCE editor and use no custom UI or a minimal generic parameter UI.

### Stage 2: stabilize compatibility

Expected files:

- same as stage 1, plus compatibility helpers if needed

Suggested work:

1. Verify file preset import/export compatibility with JUCE builds.
2. Verify external MIDI note/pitch/mod behavior against current Standalone/VST3/LV2 builds.
3. Verify mono/poly/glide semantics match the current implementation.

### Stage 3: decide whether a framework-neutral core is worth the refactor

Expected files:

- additional new engine abstraction files if the team chooses to continue beyond the minimal milestone

Suggested work:

1. Evaluate whether the JUCE-dependent extracted engine is sufficient.
2. Only then consider a framework-neutral core if long-term multi-wrapper maintenance becomes too costly.

### Stage 4: optional custom GUI

Expected files:

- entirely new DPF UI files
- likely no reuse of `src/PluginEditor.cpp` without significant adaptation

Suggested work:

1. Decide whether to:
   - keep VST2 headless/generic-editor only
   - or recreate the current visual editor approximately in DPF

## 12. Anything that may make a DPF VST2 target unsuitable

### Legal suitability

What is known from repository files:

- this repo root does not currently expose a top-level project license file; `rg --files` found only `README.md`
- the vendored JUCE files include JUCE licensing notices in their own CMake/module files

Audit conclusion:

- legal suitability is not fully determinable from this repository alone
- a DPF integration would require a separate license review because this repo does not currently document its own top-level license in inspected files
- this is not a proven blocker, but it is an open legal check

### Technical suitability

Potential issues:

- current engine is JUCE-dependent rather than framework-neutral
- current state/preset format is APVTS/XML-based
- current UI is tightly bound to JUCE GUI classes
- audio thread currently performs a waveform snapshot allocation and lock each block

None of those make DPF VST2 impossible, but they do make “drop-in wrapper only” unrealistic.

### Architectural suitability

Potential issues:

- `src/PluginProcessor.cpp` combines:
  - oscillator code
  - synth voice code
  - synth routing
  - processor orchestration
  - state handling
  - wet/dry/reverb mixing
- the existing JUCE editor also owns factory presets and QWERTY behavior that are not represented as a framework-neutral engine service

Audit conclusion:

- a DPF VST2 target is architecturally possible
- but the current repo shape strongly favors a minimal audio-engine reuse approach first, not a full UI or full framework-neutral rewrite in the first milestone

## Recommended architecture

Approach A first:

- add a DPF VST2 wrapper
- reuse the existing JUCE-based synth engine as much as possible
- extract only the minimum code necessary from `src/PluginProcessor.cpp` to make that reuse practical
- do not port the JUCE editor in phase 1

## Minimum viable VST2 milestone

- Windows x64 DPF VST2 AggregaKeys instrument
- headless or generic-editor only
- supports:
  - note on/off
  - velocity
  - pitch bend
  - mod wheel
  - mono/poly
  - glide
  - state save/restore
- does not attempt to reproduce the current JUCE visual editor yet

## Exact files expected to change in phase 1

Most likely:

- `CMakeLists.txt`
- `src/PluginProcessor.cpp`
- `src/PluginProcessor.h`

Most likely new files:

- one or more new DPF wrapper/build files for AggregaKeys only
- one or two new shared engine files if code is extracted out of `PluginProcessor.cpp`

Not recommended to change in phase 1:

- `src/PluginEditor.cpp`
- `src/PluginEditor.h`

unless a temporary preset/state helper must move out of the editor. The current JUCE UI should stay out of scope for the first milestone.

## Exact build and verification commands for Windows

These are proposed phase-1 commands, not current working DPF commands.

Assuming DPF is later added locally:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target AggregaKeys_VST2
```

Recommended verification sequence once implemented:

```powershell
cmake --build build --config Release --target AggregatronKeys_Standalone
cmake --build build --config Release --target AggregatronKeys_VST3
cmake --build build --config Release --target AggregatronKeys_LV2
cmake --build build --config Release --target AggregaKeys_VST2
```

Recommended behavioral verification:

1. Load the VST2 in a Windows host that still supports VST2.
2. Verify `Init`-equivalent parameter defaults.
3. Verify mono off: 3-note chord playback.
4. Verify mono on: legato glide.
5. Verify pitch bend and mod wheel.
6. Verify save/restore of full state chunk.
7. Verify import/export of preset XML if that compatibility goal is chosen.

## Open questions discovered from the code

1. There is no top-level project license file in the inspected repository root. Is there an intended project license for future DPF integration?
2. Should the future VST2 build preserve JUCE XML preset-file compatibility exactly, or is host-state compatibility alone sufficient?
3. Is the future VST2 build expected to support the current QWERTY keyboard behavior, or is external MIDI sufficient for phase 1?
4. Is channel-1-only UI pitch/mod behavior acceptable for the DPF build, or should the future wrapper expose channel-selectable behavior?
5. Is the current waveform snapshot feature needed in a headless VST2 build, or can it be disabled there to avoid audio-thread allocation/locking?
6. Is it acceptable for the first VST2 milestone to omit the current JUCE custom editor entirely?
7. Should the future DPF target preserve the existing `BUNDLE_ID` / URI naming conventions in some translated form, or only preserve parameter/state identity?

