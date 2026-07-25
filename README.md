# DirectPipe Windows Input Mixer (DPWIM)

DPWIM is a standalone Windows x64 VST2/VST3 audio-effect plugin that preserves
its upstream mono/stereo input and adds audio captured from selected Windows
applications or the desktop mix.

Status: `v0.2.1` public preview. It is locally software-verified, but unsigned,
not DAW-matrix-tested, and not claimed production-ready.

## Signal flow

```text
Host input -> common dry delay ----------------------+
App/Desktop -> process loopback -> FIFO/sync/offset -+-> mixed output
```

The plugin has no runtime dependency on DirectPipe. DirectPipe can load it as a
normal 64-bit VST2/VST3 effect.

![DPWIM mixer console](docs/images/dpwim-mixer-console.png)

## Current controls

- Target Latency: one common 10-250 ms timeline for dry and captured sources.
- Dry Gain: independent level for the upstream/microphone path.
- Four source slots: one selected application or desktop capture.
- Per-source ON/OFF: suspend capture and mixing without losing the selection.
- Per-source Gain: independent -60 to +12 dB adjustment for every app.
- Per-source Offset: signed -200 to +200 ms manual sync trim.
- Refresh: updates the selectable Windows process list.

New instances start at the minimum 10 ms Target Latency with Dry Gain and all
source gains at 0 dB. Saved host projects and presets restore their own values.

Each captured source has its own bounded FIFO and drift-correction clock. App
mode includes the selected process tree; desktop mode captures system output
while excluding the current host process tree to reduce feedback.

## Requirements

- Windows 10 build 20348 or later
- Visual Studio 2022 C++ toolchain
- CMake 3.22+
- Network access for the pinned JUCE fetch, or a local JUCE source override
- Optional valid VST2 SDK headers for the VST2 target

## Build

```powershell
cmake -S . -B build -DDPWIM_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

To enable VST2 without copying its SDK into the repository:

```powershell
cmake -S . -B build `
  -DDPWIM_VST2_SDK_PATH=C:/path/to/VST2_SDK `
  -DDPWIM_BUILD_TESTS=ON
```

VST2 headers are not redistributed. DPWIM source and release binaries are
licensed under GPL-3.0-only; see `LICENSE`. Release binaries are currently
unsigned.

## Local verification

The current Windows checkout has passed:

```powershell
ctest --test-dir build -C Release --output-on-failure
.\build\Release\dpwim-loopback-smoke.exe
.\build\Release\dpwim-plugin-probe.exe `
  '.\build\DPWIM_artefacts\Release\VST3\DirectPipe Windows Input Mixer.vst3' `
  '.\build\DPWIM_artefacts\Release\VST\DirectPipe Windows Input Mixer.dll'
```

The loopback smoke test plays a tone in its own process and verifies captured
frames and non-zero energy. The host probe scans and instantiates both formats,
checks the dry impulse against reported latency, and round-trips plugin state.

Manual validation in DirectPipe and representative third-party DAWs is still
required before a stable/general-compatibility claim.

## License

Copyright (c) 2026 LiveTrack-X.

DPWIM is free software licensed under the GNU General Public License version 3
only (`GPL-3.0-only`).
