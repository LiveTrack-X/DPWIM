# Changelog

All notable changes to DPWIM are documented here.

## [Unreleased]

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
