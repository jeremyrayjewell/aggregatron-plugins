# AggregaKeys VST2 Probe

This subproject is a narrow Windows x64 VST2 toolchain proof using the DISTRHO Plugin Framework (DPF). It exists only to verify that this repository can produce a genuine DPF-based VST2 instrument DLL that loads in a host such as OpenMPT. It does not contain AggregaKeys DSP, parameters, presets, state format, or UI code.

## What this probe does

- Builds a VST2 instrument only
- Uses DPF, not JUCE
- Exposes one automatable parameter: `Gain`
- Responds to MIDI note-on and note-off
- Supports simple polyphonic sine-wave playback
- Has no custom UI

## Prerequisites

- Windows x64
- Visual Studio with C++ build tools
- CMake
- Git

## Dependency setup

Initialize the pinned DPF submodule:

```powershell
git submodule update --init --recursive external/DPF
```

If you want to use a different local DPF checkout temporarily, pass `-DDPF_SOURCE_DIR="C:/path/to/DPF"` when configuring.

## Configure and build

From the repository root:

```powershell
cmake -S dpf-vst2 -B build-dpf-vst2 -A x64
cmake --build build-dpf-vst2 --config Release
```

## Expected artifact

The VST2 DLL is expected at:

```text
build-dpf-vst2/bin/AggregaKeysVST2Probe-vst2.dll
```

## Manual OpenMPT install and scan

1. Build the probe DLL.
2. Copy `build-dpf-vst2/bin/AggregaKeysVST2Probe-vst2.dll` into a folder that OpenMPT scans for VST2 plugins.
3. Open OpenMPT.
4. Go to the plugin manager / plugin settings area.
5. Add or confirm the scan path containing the DLL.
6. Run a plugin rescan.
7. Insert the probe as an instrument plugin in a test project.

## Manual test checklist

- The plugin appears during the VST2 scan.
- The plugin loads without crashing.
- A MIDI note produces sound.
- Note-off stops the voice.
- Chords work.
- Velocity changes amplitude.
- `Gain` automation changes output.
- Saving and reloading the host project preserves `Gain`.

## Non-goals in this phase

- No AggregaKeys DSP
- No AggregaKeys parameter set
- No JUCE state compatibility
- No factory presets
- No custom editor

## Attribution

This probe uses the DISTRHO Plugin Framework (DPF), which includes its own licensing and attribution files under `external/DPF/`.

DPF's VST2 compatibility layer is a clean-room implementation and does not require bundling or referencing the discontinued Steinberg VST2 SDK. See the DPF repository files, including `external/DPF/LICENSING.md`, for the relevant attribution and licensing details.
