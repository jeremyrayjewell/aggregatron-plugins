# DPF Windows UI Backend Diagnostic

Date: 2026-06-16  
Repository: `aggregatron-plugins`  
Scope: diagnostic only, no AggregaKeys or DPF source changes applied

## Summary

The native DPF UI failure is reproducible in an official DPF example outside AggregaKeys. The failure occurs before any AggregaKeys UI code would matter.

The most likely root cause is a broken/incomplete DPF checkout at the pinned revision: the bundled Khronos headers that DPF relies on for Windows OpenGL extension typedefs are present but zero bytes long:

- `external/DPF/khronos/GL/glext.h`
- `external/DPF/khronos/KHR/khrplatform.h`

Because of that, DPF/DGL compiles `NanoVG.cpp` and `OpenGL3.cpp` without the expected OpenGL extension typedefs and enums such as:

- `PFNGLACTIVETEXTUREPROC`
- `GL_TEXTURE0`
- `GL_ARRAY_BUFFER`
- `GLchar`

This is not currently attributable to AggregaKeys UI implementation code.

## DPF Revision

- DPF submodule commit: `4238e1c7f0351bbe488d79f0899c540543ac7583`

## Nested Submodule Status

Command:

```powershell
git submodule status --recursive
```

Observed status:

```text
4238e1c7f0351bbe488d79f0899c540543ac7583 external/DPF (heads/main)
5e2621d714ddf1cb0f86e852f8ba5dffe04aa3a3 external/DPF/dgl/src/pugl-upstream (remotes/origin/master-318-g5e2621d)
```

Conclusion:

- required nested DPF submodule `pugl-upstream` is initialized
- no missing or uninitialized nested submodules were observed in this recursive status output

## Generator / Toolchain Used

Primary test toolchain:

- generator: `Visual Studio 18 2026`
- platform: `x64`
- project file tool version: `18.0`
- platform toolset: `v145`
- Windows target platform version: `10.0.26100.0`
- MSBuild reported: `MSBuild version 18.4.0+6e61e96ac`

Attempted secondary toolchain:

- generator requested: `Visual Studio 17 2022`
- result: not installed / not usable on this machine

Exact VS 2022 configure failure:

```text
Generator

  Visual Studio 17 2022

could not find any instance of Visual Studio.
```

## DPF Example Tested

Primary official DPF UI example:

- `external/DPF/examples/Info`
- target built: `d_info-vst2`

Secondary confirmation build:

- `external/DPF/examples/Parameters`
- target built: `d_parameters-vst2`

Both examples exercise DPF UI code and reproduce the same backend failure.

## Configure / Build Commands

Primary fresh diagnostic build directory:

```powershell
cmake -S external\DPF -B build-dpf-ui-diagnostic-vs2026 -G "Visual Studio 18 2026" -A x64 -DDPF_EXAMPLES=ON -DDPF_LIBRARIES=ON
cmake --build build-dpf-ui-diagnostic-vs2026 --config Release --target d_info-vst2
cmake --build build-dpf-ui-diagnostic-vs2026 --config Release --target d_parameters-vst2
```

Secondary VS 2022 probe:

```powershell
cmake -S external\DPF -B build-dpf-ui-diagnostic-vs2022 -G "Visual Studio 17 2022" -A x64 -DDPF_EXAMPLES=ON -DDPF_LIBRARIES=ON
```

## Exact Failing Target

The first failing target in the stock DPF example build was:

- `dgl-opengl.vcxproj`

This failure occurred while building:

- `external/DPF/dgl/src/NanoVG.cpp`

Earlier AggregaKeys UI attempts also showed failure in:

- `dgl-opengl3.vcxproj`
- `external/DPF/dgl/src/OpenGL3.cpp`

So both the OpenGL and OpenGL3 DGL backends are implicated.

## Exact First Compiler Error

First compiler error from the official DPF example build:

```text
D:\git\aggregatron-plugins\external\DPF\dgl\src\NanoVG.cpp(34,1): error C4430: missing type specifier - int assumed. Note: C++ does not support default-int [D:\git\aggregatron-plugins\build-dpf-ui-diagnostic-vs2026\dgl-opengl.vcxproj]
```

Immediately followed by:

```text
D:\git\aggregatron-plugins\external\DPF\dgl\src\NanoVG.cpp(34,1): error C2146: syntax error: missing ';' before identifier 'glActiveTexture' [D:\git\aggregatron-plugins\build-dpf-ui-diagnostic-vs2026\dgl-opengl.vcxproj]
```

## Relevant Error Cluster

The failure cluster from the stock DPF example includes missing typedefs, functions, and enums such as:

- `PFNGLACTIVETEXTUREPROC`
- `glActiveTexture`
- `PFNGLATTACHSHADERPROC`
- `glAttachShader`
- `PFNGLBINDBUFFERPROC`
- `glBindBuffer`
- `PFNGLBUFFERDATAPROC`
- `glBufferData`
- `PFNGLBLENDFUNCSEPARATEPROC`
- `glBlendFuncSeparate`
- `GLchar`
- `GL_VERTEX_SHADER`
- `GL_FRAGMENT_SHADER`
- `GL_COMPILE_STATUS`
- `GL_LINK_STATUS`
- `GL_TEXTURE0`
- `GL_ARRAY_BUFFER`
- `GL_STREAM_DRAW`
- `GL_CLAMP_TO_EDGE`
- `GL_INCR_WRAP`
- `GL_DECR_WRAP`

Representative failing lines:

- `external/DPF/dgl/src/NanoVG.cpp:34-61`
- `external/DPF/dgl/src/nanovg/nanovg_gl.h:376, 442, 481, 482, 483, 490, 503, 509, 748, 1129, 1339, 1363, 1364`
- `external/DPF/dgl/src/OpenGL3.cpp` also previously failed with the same family of symbols

## Whether The Same Error Appears Outside AggregaKeys

Yes.

The same failure appears in official DPF example targets:

- `d_info-vst2`
- `d_parameters-vst2`

That means this is not specific to:

- `dpf-vst2/aggregakeys/`
- AggregaKeys UI source
- AggregaKeys assets
- AggregaKeys parameter/UI wiring

## Where The Missing Typedefs Should Have Been Declared

The missing extension typedefs should have been available before `NanoVG.cpp` uses them.

Relevant source path:

- `external/DPF/dgl/src/NanoVG.cpp:22`
  - includes `../NanoVG.hpp`
- `external/DPF/dgl/NanoVG.hpp:20`
  - includes `OpenGL.hpp`
- `external/DPF/dgl/OpenGL.hpp:23`
  - includes `OpenGL-include.hpp`
- `external/DPF/dgl/OpenGL-include.hpp:63-64`
  - includes `<GL/gl.h>`
  - includes `<GL/glext.h>`

The first place the missing typedefs are consumed is:

- `external/DPF/dgl/src/NanoVG.cpp:34`

```cpp
DGL_EXT(PFNGLACTIVETEXTUREPROC, glActiveTexture)
```

So by that line, `PFNGLACTIVETEXTUREPROC` should already have been declared by `GL/glext.h`.

## Where Those Symbols Are Expected To Come From

In this DPF build, the missing typedefs and enums should come from:

- `GL/glext.h`

More specifically:

- DPF’s own include path adds `external/DPF/khronos`
- generated `dgl-opengl.vcxproj` contains:

```text
AdditionalIncludeDirectories=...;D:\git\aggregatron-plugins\external\DPF\khronos;...
```

So `<GL/glext.h>` should resolve to:

- `external/DPF/khronos/GL/glext.h`

This is not expected to come from:

- `pugl`
- `NanoVG` itself
- Windows `gl.h` alone

`pugl-upstream` contains its own `glad` example files, but DPF’s active DGL compile path here is not using those headers for `NanoVG.cpp`.

## Critical Checkout Finding

The bundled Khronos headers in the pinned DPF checkout are present as regular files but have zero length:

- `external/DPF/khronos/GL/glext.h`
- `external/DPF/khronos/KHR/khrplatform.h`

Observed metadata:

```text
FullName   : D:\git\aggregatron-plugins\external\DPF\khronos\GL\glext.h
Attributes : Archive
LinkType   :
Target     : {}
Mode       : -a----
Length     : 0
```

```text
FullName   : D:\git\aggregatron-plugins\external\DPF\khronos\KHR\khrplatform.h
Attributes : Archive
LinkType   :
Target     : {}
Mode       : -a----
Length     : 0
```

Important implication:

- this is not a missing nested submodule problem
- this is not a missing AggregaKeys UI source problem
- this is not a pure host-wrapper integration issue
- the pinned DPF checkout contains unusable Khronos header payloads in this environment

## Windows / OpenGL Include Configuration Findings

Relevant DPF configuration facts:

- `UI_TYPE` defaults to `opengl` in `external/DPF/cmake/DPF-plugin.cmake`
- `opengl3` is also supported
- `dpf__add_dgl_opengl3(...)` links `dgl-opengl3`
- `dgl-opengl` and `dgl-opengl3` include:
  - `external/DPF/dgl`
  - `external/DPF/dgl/src/pugl-upstream/include`
  - `external/DPF`
  - `external/DPF/khronos`

CMake OpenGL findings in `build-dpf-ui-diagnostic-vs2026/CMakeCache.txt`:

- `OPENGL_gl_LIBRARY=opengl32`
- no useful `OPENGL_INCLUDE_DIR` value was populated in the cache excerpt

This did not prevent the project from compiling the OpenGL sources, because the generated project already includes `external/DPF/khronos`, which should have been sufficient if those headers were valid.

## Whether This Looks Like A Broken Checkout, Toolchain Issue, Or Project Misconfiguration

### 1. Broken or incomplete DPF checkout

Strong evidence: yes.

Reason:

- `external/DPF/khronos/GL/glext.h` is zero bytes
- `external/DPF/khronos/KHR/khrplatform.h` is zero bytes
- those are exactly the headers DPF relies on for the missing GL typedefs/enums

This is the strongest current explanation.

### 2. Missing Windows/OpenGL include or library configuration

Possible as a secondary factor, but not the primary blocker observed here.

Reason:

- the DPF-generated MSVC project already includes `external/DPF/khronos`
- that path should satisfy `GL/glext.h`
- the header payload itself is unusable in this checkout

### 3. Visual Studio 18 2026 compatibility issue

Unproven as the primary cause.

Reason:

- VS 2026 is the toolchain that surfaced the problem
- but the root symptom is directly explained by zero-byte Khronos headers
- VS 2022 could not be tested because no VS 2022 instance is installed

So a VS 2026-specific bug cannot be ruled out completely, but the checkout defect is sufficient to explain the compile failure already.

### 4. DPF configuration mistake in `dpf-vst2` CMake

No.

Reason:

- the same failure reproduces in a stock DPF example configured directly from `external/DPF`

### 5. Issue specific to attempted AggregaKeys UI implementation

No.

Reason:

- the stock DPF UI examples fail before any AggregaKeys UI code is involved

## Likely Root Cause

Most likely root cause:

- the pinned DPF checkout is incomplete or corrupted for the bundled Khronos OpenGL headers on this machine, specifically `external/DPF/khronos/GL/glext.h` and `external/DPF/khronos/KHR/khrplatform.h` being zero-byte files

That causes DPF/DGL Windows UI backends to compile without the extension typedefs and enums they require.

## Minimum Proposed Patch Or Workaround

Smallest credible next step:

1. restore valid contents for:
   - `external/DPF/khronos/GL/glext.h`
   - `external/DPF/khronos/KHR/khrplatform.h`
2. rebuild a stock DPF UI example first
3. only after that, retry AggregaKeys custom UI work

Practical ways to do that:

- refresh the DPF checkout at the same pinned commit from a clean source
- or update the DPF submodule to a known-good revision where these headers are intact
- or patch the pinned DPF checkout locally by restoring the correct Khronos header files

Do not proceed directly to AggregaKeys UI code until a stock DPF UI example builds.

## Risks Of Patching DPF Locally

- local divergence from the pinned DPF revision
- future merge/update friction
- possibility of masking a broader checkout or tooling problem
- reproducibility concerns for other developers if the fix is not documented precisely

## Recommendation

Primary recommendation:

- **update or refresh the DPF submodule checkout first**

If staying on this exact DPF commit is mandatory:

- **patch the pinned DPF checkout locally only by restoring the Khronos headers**

Current evidence does **not** support:

- blaming AggregaKeys UI code
- redesigning the `dpf-vst2` project
- switching away from DPF immediately
- beginning a non-DPF GUI strategy yet

If the restored headers still fail afterward, then a second diagnostic pass should revisit:

- VS 2026 compatibility
- DGL backend selection
- local DPF OpenGL loader assumptions

## Additional Commands Executed During Diagnosis

Commands used to inspect and confirm the failure:

```powershell
rg --files external\DPF\examples
Get-Content external\DPF\CMakeLists.txt
Get-Content external\DPF\examples\Info\CMakeLists.txt
rg -n "PFNGLACTIVETEXTUREPROC|GL_TEXTURE0|GL_ARRAY_BUFFER|glext.h|OpenGL-include.hpp|NanoVG.hpp" external\DPF\dgl
Get-Content external\DPF\dgl\NanoVG.hpp | Select-Object -First 120
Get-Content external\DPF\dgl\OpenGL-include.hpp | Select-Object -First 140
Get-Content external\DPF\dgl\src\NanoVG.cpp | Select-Object -First 80
rg -n "UI_TYPE|opengl3|DGL_USE_OPENGL3|DISTRHO_UI_USE_NANOVG|DISTRHO_UI_USE_CAIRO|OpenGL-include" external\DPF\cmake external\DPF\distrho
Get-Content external\DPF\cmake\DPF-plugin.cmake | Select-Object -Skip 1320 -First 90
rg -n "OPENGL_INCLUDE_DIR|OPENGL_gl_LIBRARY|CMAKE_GENERATOR" build-dpf-ui-diagnostic-vs2026\CMakeCache.txt
rg -n "AdditionalIncludeDirectories|IncludePath|glext|OpenGL-include|DGL_USE_OPENGL3|DGL_OPENGL" build-dpf-ui-diagnostic-vs2026\dgl-opengl.vcxproj
Get-Item external\DPF\khronos\GL\glext.h | Format-List FullName,Length,LastWriteTime
Get-Item external\DPF\khronos\KHR\khrplatform.h | Format-List FullName,Length,LastWriteTime
Get-Item external\DPF\khronos\GL\glext.h | Format-List FullName,Attributes,LinkType,Target,Mode,Length
Get-Item external\DPF\khronos\KHR\khrplatform.h | Format-List FullName,Attributes,LinkType,Target,Mode,Length
```

## Final Diagnostic Conclusion

The native DPF UI backend failure is reproducible in official DPF examples and is therefore not an AggregaKeys UI implementation problem.

The strongest identified root cause is a broken/incomplete DPF checkout on this machine: DPF’s bundled Khronos OpenGL headers are zero-byte files, which prevents `NanoVG.cpp` and related DGL OpenGL backend code from compiling on Windows.
