# Aggregatron Plugins

This repository contains the latest source for two JUCE audio programs:

- `AggregaKeys v0.2`: a MIDI-driven polyphonic synth with a simple control panel
- `AggregaMap v0.1`: a second JUCE plugin target built from the `samplemap/` sources

Both targets build as `Standalone` apps and `VST3` plugins from the same project.

## Repository layout

- `CMakeLists.txt`: shared JUCE/CMake build for both targets
- `src/`: `AggregaKeys` source
- `samplemap/`: `AggregaMap` source
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

Artifacts will be generated under `build/` for both products:

- `AggregaKeys v0.2`
- `AggregaMap v0.1`

## Sharing / version control notes

- `build/` is generated output and should not be committed
- local IDE settings are ignored
- the vendored local `JUCE/` checkout is ignored so the repo stays lightweight

## Current targets

### AggregaKeys v0.2

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
