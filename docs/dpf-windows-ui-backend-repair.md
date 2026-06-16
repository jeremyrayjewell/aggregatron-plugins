# DPF Windows UI Backend Repair

Date: 2026-06-16  
Repository: `aggregatron-plugins`  
Scope: repair DPF Windows UI backend only

## Result

The DPF Windows UI backend is working again on this machine.

Success was verified by:

- building a stock DPF native-UI example successfully
- rebuilding `AggregaKeysDPFVST2-vst2`
- rebuilding `AggregaKeysVST2Probe-vst2`

No AggregaKeys source files were modified in this phase.

## Original DPF Commit

- `4238e1c7f0351bbe488d79f0899c540543ac7583`

## Repaired / Updated DPF Commit

- `4238e1c7f0351bbe488d79f0899c540543ac7583`

The DPF submodule commit was **not** changed.

## Whether The Zero-Byte Headers Were In The DPF Commit

They were **not** tracked in the DPF commit.

Verification:

```powershell
git -c safe.directory=D:/git/aggregatron-plugins/external/DPF -C external/DPF cat-file -s HEAD:khronos/GL/glext.h
git -c safe.directory=D:/git/aggregatron-plugins/external/DPF -C external/DPF cat-file -s HEAD:khronos/KHR/khrplatform.h
```

Both returned:

```text
fatal: path '...' exists on disk, but not in 'HEAD'
```

Additional findings:

- `external/DPF/.gitignore` ignores `/khronos/`
- `external/DPF/cmake/DPF-plugin.cmake` creates `khronos/` and downloads these headers on demand for MSVC builds

Conclusion:

- the zero-byte files were local broken header state
- they were not committed blobs in the pinned DPF revision

## Exact Repair Method Used

DPF already contains the intended repair path in `external/DPF/cmake/DPF-plugin.cmake`:

```cmake
if(MSVC)
  file(MAKE_DIRECTORY "${DPF_ROOT_DIR}/khronos/GL")
  foreach(_gl_header "glext.h")
    if(NOT EXISTS "${DPF_ROOT_DIR}/khronos/GL/${_gl_header}")
      file(DOWNLOAD "https://www.khronos.org/registry/OpenGL/api/GL/${_gl_header}" "${DPF_ROOT_DIR}/khronos/GL/${_gl_header}" SHOW_PROGRESS)
    endif()
  endforeach()
  foreach(_khr_header "khrplatform.h")
    if(NOT EXISTS "${DPF_ROOT_DIR}/khronos/KHR/${_khr_header}")
      file(DOWNLOAD "https://www.khronos.org/registry/EGL/api/KHR/${_khr_header}" "${DPF_ROOT_DIR}/khronos/KHR/${_khr_header}" SHOW_PROGRESS)
    endif()
  endforeach()
endif()
```

Because the local files already existed as zero-byte placeholders, DPF did not re-download them automatically.

The repair performed was:

1. Keep the pinned DPF commit unchanged.
2. Replace the broken zero-byte local files with the official Khronos header contents from the same URLs DPF already references.

Exact commands used:

```powershell
cmd /c curl -L "https://www.khronos.org/registry/OpenGL/api/GL/glext.h" -o external\DPF\khronos\GL\glext.h
cmd /c curl -L "https://www.khronos.org/registry/EGL/api/KHR/khrplatform.h" -o external\DPF\khronos\KHR\khrplatform.h
```

Resulting local file sizes:

- `external/DPF/khronos/GL/glext.h`: `862276` bytes
- `external/DPF/khronos/KHR/khrplatform.h`: `11131` bytes

This was the least invasive repair because:

- no DPF source logic was changed
- no DPF commit update was required
- the official DPF-documented download source was used
- the repair matches DPF’s own intended MSVC setup path

## Stock DPF UI Example Tested

Tested official example:

- `external/DPF/examples/Info`
- built target: `d_info-vst2`

## Configure / Build Commands

Fresh stock DPF UI test:

```powershell
cmake -S external\DPF -B build-dpf-ui-repair-test -G "Visual Studio 18 2026" -A x64 -DDPF_EXAMPLES=ON -DDPF_LIBRARIES=ON
cmake --build build-dpf-ui-repair-test --config Release --target d_info-vst2
```

Fresh headless plugin rebuild after repair:

```powershell
cmake -S dpf-vst2 -B build-dpf-vst2-repair -G "Visual Studio 18 2026" -A x64
cmake --build build-dpf-vst2-repair --config Release --target AggregaKeysDPFVST2-vst2
cmake --build build-dpf-vst2-repair --config Release --target AggregaKeysVST2Probe-vst2
```

## Whether The Stock UI Example Builds

Yes.

Successful artifact:

- `build-dpf-ui-repair-test/bin/d_info-vst2.dll`
- size: `936960` bytes

## Whether `AggregaKeysDPFVST2-vst2` Still Builds

Yes.

Successful artifact:

- `build-dpf-vst2-repair/bin/AggregaKeysDPFVST2-vst2.dll`
- size: `864256` bytes

## Whether `AggregaKeysVST2Probe-vst2` Still Builds

Yes.

Successful artifact:

- `build-dpf-vst2-repair/bin/AggregaKeysVST2Probe-vst2.dll`
- size: `55808` bytes

## Compiler Warnings

Observed during successful stock UI example build:

```text
C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\winnls.h(159,9): warning C4005: 'WC_ERR_INVALID_CHARS': macro redefinition
```

Context:

- target: `dgl-opengl.vcxproj`
- source file: `external/DPF/dgl/src/WebViewWin32.cpp`
- previous definition:
  - `external/DPF/distrho/extra/WebViewWin32.hpp(54,10)`

No compiler warnings were observed in the rebuilt headless AggregaKeys or probe targets in the captured build output.

## Final Changed-File List

Files intentionally changed in this phase:

- `external/DPF/khronos/GL/glext.h`
- `external/DPF/khronos/KHR/khrplatform.h`
- `docs/dpf-windows-ui-backend-repair.md`

Important note:

- the `khronos/` directory is ignored inside the DPF submodule, so these repaired header files are local working-tree repair files, not tracked DPF Git changes

## Generated Build Directories Not To Commit

These build outputs remain untracked and should not be committed:

- `build-dpf-ui-diagnostic-vs2022/`
- `build-dpf-ui-diagnostic-vs2026/`
- `build-dpf-ui-repair-test/`
- `build-dpf-vst2-phase4/`
- `build-dpf-vst2-phase5/`
- `build-dpf-vst2-repair/`
- `build-dpf-vst2/`
- `build-phase2/`
- `build-phase3/`
- `dist/`

## Recommendation For Returning To Phase 5 UI Work

It is now reasonable to return to Phase 5 UI work, but the safest sequence is:

1. Use the repaired DPF state already verified here.
2. First rebuild a stock DPF UI example if the environment changes.
3. Then retry the AggregaKeys DPF custom UI implementation.

Additional note:

- because `external/DPF/khronos/` is ignored and populated dynamically, a future reproducibility hardening step may still be worthwhile
- the most targeted hardening would be teaching the DPF CMake download step to re-download zero-byte headers, not just missing files
- that hardening was **not** implemented in this phase
