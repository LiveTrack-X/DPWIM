# DPWIM Open Implementation Items

Status: Active

## Active Work

No active work. DPWIM-0002 is locally software-verified.

## Future / Deferred

- [packet:DPWIM-0001] Before a stable/general-compatibility claim, run manual
  UI/audio validation in DirectPipe and a representative third-party DAW,
  including desktop exclusion and unload.
- DirectPipe built-in integration requires a separate owner-selected packet.
- Shared capture coordination across multiple plugin instances.
- Dynamic offset crossfade and timestamp-calibrated sample alignment beyond the
  first bounded PLL implementation.
- Installer, signing, and broader DAW compatibility matrix.

## Release / Production Readiness

The owner authorized a public `LiveTrack-X/DPWIM` repository, GPL-3.0-only,
and an unsigned `v0.1.0` preview release on 2026-07-26. Code signing and a
stable release remain deferred.

## Recently Closed

- [x] [packet:DPWIM-0001] Implemented standalone VST2/VST3 builds and Windows
  process-loopback capture.
- [x] [packet:DPWIM-0001] Implemented four independent source gains, common
  target latency, dry delay, signed ms offsets, and bounded PLL drift control.
- [x] [packet:DPWIM-0001] Implemented process selection, state persistence, and
  best-effort executable reattachment.
- [x] [packet:DPWIM-0001] Release build, deterministic tests, live process
  loopback smoke, and JUCE VST2/VST3 host probes passed locally.
- [x] [packet:DPWIM-0002] Implemented the selected native mixer-console editor,
  dynamic executable icons, and resizable reference/minimum layouts.
- [x] [packet:DPWIM-0002] Captured the real JUCE editor and passed visual QA
  with no remaining P0-P2 findings.
