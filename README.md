# DirectPipe Windows Input Mixer (DPWIM)

> Mix and synchronize your DAW input with Windows application audio in one
> plugin.
>
> 하나의 플러그인에서 DAW 입력과 Windows 애플리케이션 오디오를 믹스하고
> 동기화하세요.

## DirectPipe 

**[DirectPipe](https://github.com/LiveTrack-X/DirectPipe)** is a cross-platform
real-time VST2/VST3/AU host with plugin-chain processing, fast preset switching,
and external control through hotkeys, MIDI, Stream Deck, WebSocket, and HTTP.

**[DirectPipe](https://github.com/LiveTrack-X/DirectPipe)**는 VST2/VST3/AU
플러그인 체인, 빠른 프리셋 전환, 단축키·MIDI·Stream Deck·WebSocket·HTTP
제어를 지원하는 실시간 오디오 플러그인 호스트입니다.

DPWIM can be loaded by DirectPipe as a regular 64-bit VST2/VST3 effect, but it
does not require DirectPipe and can run independently in another compatible
DAW.

DPWIM은 DirectPipe에서 일반 64-bit VST2/VST3 플러그인으로 사용할 수 있지만,
DirectPipe가 필수는 아니며 다른 호환 DAW에서도 독립적으로 사용할 수
있습니다.

## About DPWIM / DPWIM 소개

**DirectPipe Windows Input Mixer (DPWIM)** is a standalone Windows x64
VST2/VST3 audio-effect plugin that preserves the existing microphone or plugin
chain input and mixes in audio captured from Windows applications or the
desktop.

It provides independent gain, ON/OFF, and ±200 ms synchronization controls for
up to four sources without requiring a virtual audio device. DPWIM is currently
a free GPL-3.0 public preview.

**DirectPipe Windows Input Mixer(DPWIM)**는 기존 마이크·플러그인 체인 입력을
유지하면서 Windows 애플리케이션 또는 데스크톱 오디오를 직접 합성하는
Windows x64 VST2/VST3 플러그인입니다.

최대 4개 소스의 음량, ON/OFF, ±200 ms 싱크를 각각 조절할 수 있으며 별도의
가상 오디오 장치 없이 사용할 수 있습니다. 현재 GPL-3.0 기반의 무료 공개
프리뷰입니다.

**[Download DPWIM v0.2.2 / DPWIM v0.2.2 다운로드](https://github.com/LiveTrack-X/DPWIM/releases/tag/v0.2.2)**

Status: `v0.2.2` public preview. It is locally software-verified, but unsigned,
not DAW-matrix-tested, and not claimed production-ready.

상태: `v0.2.2` 공개 프리뷰입니다. 로컬 소프트웨어 검증은 통과했지만
코드 서명과 광범위한 DAW 호환성 검증은 아직 완료되지 않았습니다.

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

## AI-assisted development / AI 보조 개발

DirectPipe is developed with substantial assistance from AI coding tools,
primarily Claude and Codex. The initial ideas, product direction, and basic
structure of selected documents are owner-led; scope, priorities, and final
acceptance decisions remain owner-led as well.

DirectPipe uses the
[SDAD Protocol](https://github.com/LiveTrack-X/spec-driven-ai-development)
(SPEC-Directed AI Development), developed alongside this work, as a
repository-local development control layer. It keeps the active scope,
validation evidence, unresolved findings, and owner gates for actions such as
releases or public compatibility changes explicit. SDAD controls the
development workflow only and is not part of the DirectPipe runtime.
AI-assisted output is reviewed and validated where possible, but AI use does
not itself guarantee correctness or replace owner responsibility.

DirectPipe는 주로 Claude와 Codex 같은 AI 코딩 도구의 상당한 보조를 받아
개발합니다. 초기 아이디어, 제품 방향, 일부 문서의 기본 구조는 소유자가
정하며, 범위·우선순위·최종 승인 결정도 소유자가 맡습니다.

이 작업과 함께 개발한
[SDAD Protocol](https://github.com/LiveTrack-X/spec-driven-ai-development)
(SPEC-Directed AI Development)을 저장소 내부의 개발 제어 계층으로
사용합니다. SDAD는 현재 범위, 검증 증거, 미해결 항목, 릴리즈나 공개 호환성
변경 같은 행동의 소유자 게이트를 명시적으로 관리합니다. SDAD는 개발
워크플로만 제어하며 DirectPipe 런타임의 일부가 아닙니다. AI 보조 산출물은
가능한 범위에서 검토·검증하지만, AI 사용 자체가 정확성을 보장하거나
소유자의 책임을 대체하지는 않습니다.
