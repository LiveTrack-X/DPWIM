# DPWIM Implementation Notes

Status: Active

## IMPL-0001 - Independent product boundary

- Date: 2026-07-26
- Applies to: DPWIM-0001
- Decision: DPWIM is a separate Git repository and standalone plugin. DirectPipe
  is a validation host and possible future consumer, not a runtime dependency.
- Why: Preserve DirectPipe compatibility and allow use in other VST hosts.
- Verification impact: DirectPipe source must remain untouched; DPWIM requires
  its own build, tests, and plugin smoke evidence.

## IMPL-0002 - One common timeline with per-source trims

- Date: 2026-07-26
- Applies to: sync engine
- Decision: Delay the dry input once by a common target and let every source
  apply a signed millisecond offset relative to that target.
- Why: Avoid cumulative dry delay and provide understandable manual correction.
- Verification impact: Tests must cover offset sign, clamping, target fill, and
  reported latency.

## IMPL-0003 - Official Windows sample as API reference

- Date: 2026-07-26
- Applies to: Windows process-loopback activation
- Decision: Implement against the documented Windows process-loopback API,
  using Microsoft's MIT-licensed ApplicationLoopback sample as a behavior
  reference while keeping DPWIM-specific lifecycle and buffering code local.
- Why: The Windows API is asynchronous and its COM lifetime requirements are
  easy to implement incorrectly.
- Verification impact: Preserve the Microsoft notice and validate activation,
  stop, and host-unload paths.

## IMPL-0004 - Independent source gain and audio-thread boundary

- Date: 2026-07-26
- Applies to: mixer and UI
- Decision: Give the upstream path one Dry Gain and every app/desktop slot its
  own -60 to +12 dB Gain. Cache all parameter atomics before processing and
  report host latency from a non-audio timer.
- Why: Multiple apps require independent balancing, while parameter lookup and
  host latency notifications must stay outside the real-time callback.
- Verification impact: Host probe must process a dry impulse through both
  formats; representative DAW testing must exercise every source gain.

## IMPL-0005 - Native mixer-console editor

- Date: 2026-07-26
- Applies to: DPWIM-0002
- Decision: Use one Dry Input master strip and four equal vertical source
  strips with custom JUCE rotary controls, dynamic executable icons, concise
  capture states, an automatic build-version badge, and a resizable 1000 x 620
  reference layout. Read the badge from `ProjectInfo::versionString` so the UI
  cannot drift from the CMake/JUCE plugin version.
- Why: The prior horizontal form made app-to-app gain and sync comparison slow.
  The selected console direction keeps every source's identity, Gain, Offset,
  and state in one consistent scan path.
- Verification impact: Render the real native editor at reference and minimum
  sizes, compare the reference-size capture against the selected design, and
  rerun all audio and plugin probes.
