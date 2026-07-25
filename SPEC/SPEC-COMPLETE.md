# DirectPipe Windows Input Mixer (DPWIM) Integrated SPEC

Status: Active integrated baseline
Effective packet: DPWIM-0001

## Product Definition

DPWIM is an independent Windows x64 VST2/VST3 audio-effect plugin. It preserves
the host's mono or stereo input, captures audio rendered by selected Windows
applications or the desktop mix, synchronizes those asynchronous sources to one
target timeline, and mixes them into the outgoing plugin buffer.

DPWIM has no runtime dependency on DirectPipe. DirectPipe is one supported host,
not an IPC peer or required component.

## Origin And Owner Direction

The owner selected a standalone plugin instead of modifying DirectPipe. The
final product and repository name is `DirectPipe Windows Input Mixer`, abbreviated
`DPWIM`. The reference behavior is functional only; third-party naming, UI,
text, code, and assets are not to be copied.

## Active Scope

- Windows 10 build 20348 or later, x64.
- VST3 build by default.
- VST2 build when a separately supplied, valid VST2 SDK is available.
- Mono and stereo input/output effect layouts.
- Preserve upstream audio and add captured audio.
- Four application/desktop source slots in the first product slice.
- Per-source gain, mute-by-Off selection, and manual sync offset in milliseconds.
- Common target latency with dry-input alignment.
- Process-tree include capture for an application.
- Host-process-tree exclude capture for desktop mode.
- App process refresh, executable-name persistence, and best-effort reattach.
- Real-time-safe plugin callback: no COM, waits, locks, allocation, or process
  enumeration on the audio thread.
- State serialization for project/preset restore.

## Signal Contract

For each host block:

```text
output = delayedDryInput * dryGain
       + sum(synchronizedSource[i] * sourceGain[i])
```

All enabled sources use one common target latency. A source offset is applied
relative to that target:

- positive milliseconds delay that source further;
- negative milliseconds advance it within the buffered common timeline;
- an unavailable negative advance is clamped and surfaced as a degraded state.

The dry path is delayed once by the common target latency. The plugin reports
that dry-path latency to the VST host.

## Capture And Synchronization

- Application mode uses Windows process-loopback include-tree activation.
- Desktop mode uses process-loopback exclude-tree activation with the current
  plugin host PID to reduce self-capture feedback.
- Capture runs on worker threads and publishes float PCM into bounded SPSC
  buffers.
- Each source has an independent read clock and fractional interpolation.
- A bounded PLL adjusts consumption ratio around unity to hold target fill
  without routine frame dropping or repetition.
- Discontinuity, overflow, target loss, and restart re-prime only the affected
  source and use fades where implemented.
- Runtime offset changes must avoid hard read-pointer jumps in the final product.

## UI And State

The first UI exposes:

- common Target Latency;
- Dry Gain;
- four source rows;
- source selector with Off, Desktop, and current processes;
- source Gain and Offset;
- Refresh;
- capture status.

State stores schema version, common latency, dry gain, and each slot's mode,
PID hint, executable identity, gain, and offset. A stale PID is not authority;
the executable identity is used for best-effort reattachment.

## Non-Goals For DPWIM-0001

- macOS or Linux capture.
- Virtual audio driver installation.
- Per-browser-tab capture guarantees.
- DRM/protected-content bypass.
- Capture of exclusive-mode streams not exposed by Windows process loopback.
- Public GitHub repository creation, code signing, installer publication, or
  release publication.
- Final commercial/open-source license selection.
- DirectPipe built-in feature integration.
- Cross-host or cross-process synchronization between separate DPWIM instances.
- Claiming hardware-verified, release-ready, or production-ready behavior from
  compilation and deterministic tests alone.

## Risk Domains

- Windows COM and asynchronous activation lifetime.
- DAW unload/reload and capture-thread shutdown.
- Independent audio clocks and buffer drift.
- Desktop feedback when downstream audio is rendered by another process.
- Duplicate capture when desktop mode overlaps app slots.
- VST2 SDK redistribution and distribution rights.
- JUCE and project licensing.
- Real-time safety, underrun behavior, and gain staging.

## Acceptance Criteria For DPWIM-0001

1. A clean Windows Release build produces a VST3 artifact.
2. When a valid local VST2 SDK is supplied, the same build produces a VST2
   artifact without copying SDK headers into this repository.
3. Deterministic tests cover FIFO wrap/overflow recovery, common dry delay,
   source offset direction, PLL bounds, and state round-trip.
4. A supported host can scan and instantiate the VST3 artifact.
5. The plugin preserves upstream input with all source slots Off.
6. A selected ordinary Windows audio process can be captured and mixed in a
   live local test, or the exact environment block is recorded.
7. Desktop mode excludes the current host process tree in implementation and
   receives a live local test, or the exact environment block is recorded.
8. No release, signing, publishing, or production claim is made without the
   corresponding owner gate and evidence.

## Release Gate

Release requires owner selection of license, product identifiers, signing
identity, repository visibility, supported-host matrix, installer policy, and
manual validation on representative Windows hardware. Software verification is
not owner acceptance.

For `v0.1.0`, the owner authorized a public `LiveTrack-X/DPWIM` repository,
GPL-3.0-only licensing, and an unsigned preview release on 2026-07-26. The
preview must disclose the missing representative-DAW/manual validation.
Signing, a supported-host matrix, installer publication, and stable or
production-ready claims remain gated.

## DPWIM-0001 Local Evidence

Recorded 2026-07-26 on Windows 10.0.26200:

- Release configuration produced the x64 VST2 DLL and VST3 bundle.
- Deterministic tests passed for dry delay, signed offset direction, bounded
  PLL ratio, FIFO wrap, and overflow re-prime.
- A process-loopback smoke test rendered a one-second tone and captured 67,200
  frames with non-zero energy.
- A JUCE probe scanned and instantiated both formats, observed the dry impulse
  at the reported 2,400-sample latency, and restored serialized state.

This is software verification only. Desktop-exclude behavior, interactive UI,
unload/reload, and listening quality still require representative DAW tests.

## DPWIM-0002 Mixer Console UI

The owner selected the first Product Design direction on 2026-07-26. The native
JUCE editor must adopt its mixer-console hierarchy while preserving the
`v0.1.0` audio and state contracts:

- one compact header with DPWIM identity, sample-rate context, and Refresh Apps;
- one compact version badge sourced from the plugin build metadata;
- one Dry Input master strip with Dry Gain and Target Latency;
- four equal source channel strips arranged for side-by-side comparison;
- application selector, capture status, Gain, and Sync Offset in every strip;
- active/off state contrast using a restrained cyan-on-charcoal visual system;
- resizable desktop plugin layout with an accessible minimum size;
- no new audio routing, mute, solo, metering, EQ, or synchronization behavior.

Acceptance requires a Release build, existing core/loopback/plugin probes,
a rendered native-editor screenshot, and a visual QA comparison against the
selected source image with no remaining P0-P2 findings.

## DPWIM-0003 Default Mix And Source Enable State

New plugin instances must start with Target Latency at its supported minimum
of 10 ms and every source gain at 0 dB. Existing project and preset state
continues to restore its saved values.

Each source strip must expose an independent ON/OFF control. Turning a source
off stops its capture worker and removes it from the mix without deleting its
application/desktop mode, executable identity, gain, or sync offset. Turning it
back on resumes the preserved selection. The enable state must round-trip with
plugin state; legacy states without the field infer enabled for configured
sources and disabled for Off sources.

Application icons must be refreshed immediately after a selector change when
Windows exposes the executable path. Protected or inaccessible processes may
remain without an icon and must not receive a fabricated replacement.

Acceptance requires default-value checks through both the processor and hosted
VST formats, source-enable state round-trip coverage, the existing audio probes,
and reference/minimum native-editor captures with no P0-P2 layout regression.
