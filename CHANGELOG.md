# Changelog

All notable changes to DPWIM are documented here.

## [Unreleased]

## [0.3.3] - 2026-07-31

### Fixed

- Active Windows-capture sources now reserve at least 20 ms on the shared
  timeline. At 48 kHz this raises the ordinary active-source floor from
  514 to 960 samples, leaving enough headroom for a late shared-mode capture
  packet without muting a host block.
- The 10 ms / 480-sample minimum remains unchanged when no capture source is
  enabled. Dry Input, captured sources, footer OUT/SYNC values, and host PDC
  continue to use one aligned effective timeline.
- Added a deterministic late-packet regression that withholds capture delivery
  across three consecutive 256-frame host callbacks.

## [0.3.2] - 2026-07-31

### Fixed

- Active captured sources now reserve a resampler-aware host-block safety
  floor. A 512-frame callback requires 514 samples and the 8192-frame
  real-time reserve requires 8209, keeping FIFO reads and reported PDC aligned.
- Larger-than-prepared callbacks publish only an atomic observation from the
  audio thread. The 60 Hz message timer notifies the new host PDC before the
  next audio callback adopts the floor; until then, a captured source block is
  fully muted and re-primed even when the FIFO already holds a complete block.
- FIFO underrun, overflow, explicit discard, and WASAPI data discontinuity now
  invalidate stale history, reset sync/pitch state as needed, and fade captured
  audio back in. Generation changes are checked throughout rendering so one
  source block cannot mix samples from both sides of a discontinuity.
- Capture discard no longer rewinds the producer frame counter, avoiding a
  producer/consumer race while an active capture callback is writing.
- Update-check request publication and shutdown are now serialized so a worker
  cannot publish into a stopped request.

## [0.3.1] - 2026-07-29

### Fixed

- Level meters now repaint at 60 Hz instead of 5 Hz while source/status and
  update UI work remains throttled to 5 Hz.
- Peak and clip hold durations remain time-correct at the higher refresh rate,
  and completed update checks are removed from subsequent timer work.
- External host bypass is now separate from DPWIM's persistent internal
  bypass. Its raw audio follows the unchanged OUT delay when the host invokes
  bypass processing, so audio timing and the PDC report remain consistent.
- Internal DPWIM bypass still passes raw input immediately and reports zero
  plugin latency; its latency transition now updates immediately on the
  message thread and safely defers audio-thread automation.
- Base Latency, Sync Offset, pitch, and source-enable changes now update the
  host latency report immediately instead of depending on the 10 Hz fallback.
- Bypass transitions and callback gaps invalidate stale delay history without
  interrupting a continuous normal-to-host-bypass Dry stream.
- The Dry delay capacity now covers the complete supported Base, negative
  Offset, and pitch-latency combination at low sample rates.
- Source replacement and disable operations now invalidate FIFO history
  without mutating audio-consumer state from the UI thread.
- Latency tooltips now clarify that OUT is DPWIM's host-reported delay, not
  measured end-to-end host or device latency.

## [0.3.0] - 2026-07-29

### Added

- Automatic common-timeline rebase for negative source offsets, with effective
  output latency reported to the host.
- Header OUT indicator for effective milliseconds, added sync latency, and
  reported samples.
- User-facing `Base Latency` naming and explanatory tooltips while preserving
  the existing `targetLatency` parameter ID and saved-state compatibility.
- Per-source Advanced panel with Transpose and Fine Pitch controls.
- Subtle `Created by LiveTrack` repository footer that becomes one orange
  latest-release link when a newer GitHub release is available.
- Host-automatable global BYPASS button that immediately passes the raw host
  input at unity and reports zero plugin latency.
- Compact horizontal L/R peak meters for Dry Input and every source, with an
  adaptive dBFS scale, peak hold, and clip indication.
- Dedicated vertical MAIN OUT L/R meter at the far right, measuring the final
  post-mix or immediate bypass output.
- Deterministic coverage for sync rebasing, pitch shifting, and latency
  reporting, plus Dry/bypass meter snapshots.

### Changed

- Offset and processing-latency changes now re-prime captured audio with a
  short fade and crossfade the Dry Input delay tap.
- Selected application icons are regular child components so modal Advanced
  panels paint above them correctly.
- Sync Offset dials use a bounded secondary-control size instead of expanding
  into all remaining vertical space in large plugin windows.
- Update checks are informational only; failed or offline checks remain silent
  and plugin files are never replaced automatically.

### Known limitations

- Pitch quality still requires validation in representative DAWs and real
  monitoring routes.

## [0.2.3] - 2026-07-26

### Added

- Independent Dry Input ON/OFF control for the upstream or microphone path.
- Host-automatable and state-persistent `Dry Input Enabled` parameter.
- Hosted VST2/VST3 coverage proving Dry Input defaults ON and produces silence
  when disabled.

### Changed

- Dry Gain controls are dimmed while Dry Input is disabled.
- Application and desktop capture sources continue mixing when Dry Input is
  disabled.

## [0.2.2] - 2026-07-26

### Added

- Independent ON/OFF control for every source while preserving its selected
  application or desktop identity.
- Processor-state regression coverage for source enable persistence.

### Changed

- New instances default to 10 ms Target Latency and 0 dB on every source gain.
- Application icons refresh immediately after selecting a source.

## [0.2.1] - 2026-07-26

### Added

- Compact plugin-version badge in the mixer header.

### Changed

- Bound the visible version directly to the CMake/JUCE build metadata so the
  interface, VST2 DLL, and VST3 module report the same version.

### Fixed

- Reserved a stable icon column in application selectors so executable icons
  cannot cover the first letters of process names.

### Verification

- Windows x64 VST2/VST3 Release build and deterministic core tests.
- Real process-loopback rendered-tone smoke test.
- JUCE VST2/VST3 scan, instantiate, latency, processing, and state probe.
- Native editor visual QA at reference and minimum sizes.
- SDAD Doctor 3.2.2 strict.

## [0.2.0] - 2026-07-26

### Added

- Native mixer-console editor with one Dry Input strip and four source strips.
- Real Windows executable icons for selected application sources.
- Reference-size and minimum-size editor snapshot coverage.

### Changed

- Reorganized application, capture status, gain, and sync controls for faster
  per-source comparison.
- Added host-resizable editor support from 840 x 520 through 1280 x 800.
- Improved duplicate process-name handling while keeping executable identity
  persistence unchanged.

### Verification

- Windows x64 VST2/VST3 Release build and deterministic core tests.
- Real process-loopback rendered-tone smoke test.
- JUCE VST2/VST3 scan, instantiate, latency, processing, and state probe.
- Native editor visual QA at reference and minimum sizes.
- SDAD Doctor 3.2.2 strict.

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
