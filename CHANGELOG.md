# Changelog

All notable changes to DPWIM are documented here.

## [0.1.0] - 2026-07-26

### Added

- Windows x64 VST2 and VST3 audio-effect builds.
- Four independently adjustable application or desktop capture slots.
- Per-source gain and signed millisecond sync offset.
- Independent Dry Gain for upstream microphone or chain audio.
- Common target latency and bounded FIFO/PLL drift correction.
- Windows process-tree loopback and desktop capture excluding the host tree.
- Process refresh, executable identity persistence, and state restoration.
- Deterministic sync tests, real loopback smoke test, and VST2/VST3 host probe.

### Known limitations

- Release binaries are unsigned.
- Representative DirectPipe and third-party DAW UI/unload testing is pending.
- Multiple DPWIM instances do not share capture workers.
- Windows 10 build 20348 or later is required for process loopback.
