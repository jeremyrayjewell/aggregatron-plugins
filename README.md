# Aggregatron Plugins

This repository contains the latest source for four JUCE audio programs:

- `AggregaKeys v0.3`: a MIDI-driven polyphonic synth with a simple control panel
- `AggregaMap v0.1`: a second JUCE plugin target built from the `samplemap/` sources
- `AggregaScale v0.1`: a scale visualiser that can be set manually or inferred from an audio file
- `AggregaVocoder v0.2`: a live vocal pitch-correction effect that snaps input to the nearest note in a chosen key and scale

All plugin targets build as `Standalone` apps and `VST3` plugins from the same project.

## Repository layout

- `CMakeLists.txt`: shared JUCE/CMake build for both targets
- `src/`: `AggregaKeys` source
- `samplemap/`: `AggregaMap` source
- `scalescope/`: `AggregaScale` source
- `vocoder/`: `AggregaVocoder` source
- `assets/`: shared bundled images and font

## JUCE dependency

This repo does not commit the local `JUCE/` checkout. Use one of these options:

Option 1: clone JUCE next to this project:

```powershell
git clone https://github.com/juce-framework/JUCE.git JUCE
```

Option 2: point CMake at an existing JUCE checkout:

```powershell
cmake -S . -B build -DJUCE_DIR="C:/path/to/JUCE"
```

## Build on Windows with Visual Studio

```powershell
cmake -S . -B build -DJUCE_DIR="C:/path/to/JUCE"
cmake --build build --config Debug
```

Artifacts will be generated under `build/` for all products:

- `AggregaKeys v0.3`
- `AggregaMap v0.1`
- `AggregaScale v0.1`
- `AggregaVocoder v0.1`

## Sharing / version control notes

- `build/` is generated output and should not be committed
- local IDE settings are ignored
- the vendored local `JUCE/` checkout is ignored so the repo stays lightweight

## Current targets

### AggregaKeys v0.3

- polyphonic sine-wave synth
- ADSR envelope
- gain control
- on-screen keyboard for testing without hardware
- pitch bend support
- mod-wheel vibrato support

### AggregaMap v0.1

- JUCE plugin target defined in the shared CMake project
- source lives in `samplemap/`
- builds as both `Standalone` and `VST3`

### AggregaScale v0.1

- manual root + scale selection
- custom pitch-class selection across all twelve notes
- audio-file analysis to infer a likely scale from stable pitched frames
- visual keyboard that highlights root notes and in-scale notes across multiple octaves

### AggregaVocoder v0.2

- live pitch correction for monophonic vocal or instrument input
- root note and scale selection directly in the plugin UI
- incoming pitch is detected and pulled toward the nearest allowed note
- adjustable correction amount, snap speed, tracking window, tone, mix, and output level
