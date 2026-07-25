# DPWIM Open Implementation Items

Status: Active

## Active Work

No active work. DPWIM-0001 is locally software-verified.

## Future / Deferred

- [packet:DPWIM-0001] Before owner acceptance or release, run manual UI/audio
  validation in DirectPipe and a representative third-party DAW, including
  desktop exclusion and unload.
- DirectPipe built-in integration requires a separate owner-selected packet.
- Shared capture coordination across multiple plugin instances.
- Dynamic offset crossfade and timestamp-calibrated sample alignment beyond the
  first bounded PLL implementation.
- Installer, signing, and broader DAW compatibility matrix.

## Release / Production Readiness

Owner must decide licensing, repository visibility, signing, and distribution
before any public release.

## Recently Closed

- [x] [packet:DPWIM-0001] Implemented standalone VST2/VST3 builds and Windows
  process-loopback capture.
- [x] [packet:DPWIM-0001] Implemented four independent source gains, common
  target latency, dry delay, signed ms offsets, and bounded PLL drift control.
- [x] [packet:DPWIM-0001] Implemented process selection, state persistence, and
  best-effort executable reattachment.
- [x] [packet:DPWIM-0001] Release build, deterministic tests, live process
  loopback smoke, and JUCE VST2/VST3 host probes passed locally.
