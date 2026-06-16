# AggregaKeys DPF VST2

This target is the first real headless DPF-based VST2 build of the actual AggregaKeys synth engine. It reuses the extracted JUCE-based engine and parameter snapshot, but it does not use the JUCE plugin wrapper or the JUCE custom editor.

## Relationship to existing builds

- The existing JUCE `Standalone`, `VST3`, and `LV2` builds remain the main reference implementations.
- This DPF target is a separate headless VST2 build under `dpf-vst2/`.
- The temporary `AggregaKeysVST2Probe` target remains available for isolated toolchain validation.

## Build prerequisites

- Windows x64
- Visual Studio with C++ build tools
- CMake
- Git
- A local DPF checkout at `external/DPF`
- A local JUCE checkout at `JUCE/` or another path passed via `JUCE_SOURCE_DIR`

## Configure and build

From the repository root:

```powershell
cmake -S dpf-vst2 -B build-dpf-vst2-phase4 -G "Visual Studio 18 2026" -A x64
cmake --build build-dpf-vst2-phase4 --config Release --target AggregaKeysDPFVST2-vst2
cmake --build build-dpf-vst2-phase4 --config Release --target AggregaKeysVST2Probe-vst2
```

## Expected artifact

```text
build-dpf-vst2-phase4/bin/AggregaKeysDPFVST2-vst2.dll
```

## OpenMPT manual test steps

1. Build the DLL.
2. Copy `build-dpf-vst2-phase4/bin/AggregaKeysDPFVST2-vst2.dll` into an OpenMPT VST2 scan folder.
3. Rescan plugins in OpenMPT.
4. Insert the plugin as an instrument.
5. Open the host's generic parameter editor if needed.
6. Test note input from the tracker keyboard and external MIDI.
7. Verify chords, mono mode, glide, pitch bend, mod wheel, and reverb-related parameters.

## Known limitations

- No custom GUI yet
- No JUCE XML preset import/export yet
- Factory presets deferred
- Host generic editor only
- Parameter state is host-managed only for this phase
- If a host delivers a block larger than the advertised buffer-size hint, the wrapper may need to grow internal JUCE buffers during audio processing
