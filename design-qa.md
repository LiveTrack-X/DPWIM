# DPWIM Mixer Console Design QA

Final result: passed

## Comparison Target

- Source visual truth:
  `C:\Users\livet\.codex\generated_images\019f9aa0-937e-77e2-b3d9-d7bf980485d0\call_htXe4rGp6EbWM3J7bDnHc65f.png`
- Implementation screenshot:
  `C:\Users\livet\Desktop\DPWIM\docs\images\dpwim-mixer-console.png`
- Minimum-size screenshot:
  `C:\Users\livet\Desktop\DPWIM\out\ui\dpwim-mixer-console-minimum.png`
- State: 48 kHz; three application sources active; source 4 Off; gains
  -6/-12/+3 dB; offsets +15/-25/+8 ms.

## Viewport And Normalization

- Source pixels: 1594 x 986.
- Implementation pixels and CSS-equivalent native size: 1000 x 620 at 1x.
- Minimum native size: 840 x 520 at 1x.
- Both reference-size images use the same approximately 1.615:1 aspect ratio.
  The source was visually normalized to the implementation width for the
  full-view comparison; no device or browser chrome was present.

## Full-View Comparison Evidence

- The implementation preserves the selected hierarchy: compact header, Dry
  Input master strip, and four equal source strips.
- Application, capture status, Gain, and Sync Offset occupy the same vertical
  scan order in every source strip.
- Cyan active values, charcoal surfaces, quiet separators, and dimmed Off state
  match the source visual system.
- Windows supplies the real executable icons. Runtime process names replace the
  mock Spotify/Chrome/Discord labels when those exact apps are unavailable.
- The minimum-size capture keeps every persistent control visible with no
  overlap, clipping, horizontal overflow, or two-line selector text.

Focused region comparison was not required because all header, selector,
status, knob, value, and label details are readable at the original full-view
pixel sizes.

## Required Fidelity Surfaces

- Fonts and typography: native Segoe UI-style rendering, two practical weights,
  stable label hierarchy, fitted one-line selector text, and no truncation of
  essential control values.
- Spacing and layout rhythm: reference-sized master/channel proportions and
  consistent vertical strip rhythm; minimum layout remains intact.
- Colors and visual tokens: restrained cyan accent, high-contrast foreground,
  neutral dividers, and disabled gray state closely follow the source.
- Image quality and asset fidelity: executable icons are extracted from Windows
  at runtime and rendered at native resolution. No third-party logo assets,
  placeholder icons, or approximate brand drawings are bundled.
- Copy and content: DPWIM, Windows Input Mixer, Refresh apps, Dry Input,
  Application, Gain, Sync Offset, Capturing, and Off remain explicit.

## Comparison History

### Pass 1

- [P2] The initial resizable editor exposed a corner resize grip not present in
  the selected visual.
- [P2] Text-only process identity did not preserve the selected design's rapid
  app recognition.
- [P2] A long executable name could wrap at the 840 x 520 minimum.

Fixes:

- Removed the embedded resize grip while retaining host-driven resizing.
- Added Windows-provided executable icons to selected application controls.
- Constrained selector labels to a fitted single line.

### Pass 2

Post-fix evidence:

- `docs/images/dpwim-mixer-console.png` matches the selected composition at the
  reference viewport.
- `out/ui/dpwim-mixer-console-minimum.png` has no P0-P2 responsive defects.
- No actionable P0, P1, or P2 difference remains.

## Accepted Product Constraints

- Target Latency remains an adjustable rotary control instead of the mock's
  value-only treatment.
- Parameter endpoint tick text is omitted because the source mock's illustrative
  ranges differ from DPWIM's actual -60 to +12 dB and -200 to +200 ms ranges.
- An Off source displays its stored disabled values so re-enabling the source
  does not silently reset the mix.

## Follow-Up Polish

- [P3] A future pass may add icons inside the open process popup, not only the
  selected closed control.
