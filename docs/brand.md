# Brand: FROTH Audio

The brand for the plugin suite. "FROTH" is not a registered trademark for audio software as far as the search done when choosing it went, but run a proper clearance search (USPTO, and EUIPO if you sell in Europe) before publishing. The word "Pulsar" was dropped because it is a registered trademark.

Change `BRAND_WORD` and `BRAND_SUFFIX` in `tools/brand/make_brand.py` to rename it, then regenerate the artwork.

## Parts

- **Wordmark**: "FROTH" in bold type with the accent colour as a gradient, so each plugin's lockup carries its own colour. The O is a bubble: a ring, a smaller bubble inside it, and a white glint on the top left. "Audio" follows in neutral type.
- **Mark**: a cluster of three bubbles with glints, in pale blue-white. File: `modules/suite_ui/brand/brand_mark.png`.
- **Lockup**: the wordmark above the product name in neutral spaced type, with the mark on the left. One lockup per plugin:
  - `lockup_spectral_drift.png`, cyan `#2DE2FF`
  - `lockup_dynamic_saturation.png`, amber `#FF9F1C`
  - `lockup_micro_timing_chaos.png`, lime `#7DFF5A`

## Typeface

Bahnschrift Bold, the Windows system font, is used to draw the artwork. The artwork is a bitmap, so the font itself is not shipped. Check the font's licence before selling the suite.

## Using it

- Each plugin embeds its own lockup with `juce_add_binary_data` (see the `*Assets` targets in each plugin's `CMakeLists.txt`).
- The editor draws it at the left of the title row with `SuiteUI::drawBrandLockup()` (`modules/suite_ui/include/SuiteBrand.h`).
- The artwork is rendered at 4x the on-screen size (about 40 px high in the editors), so it stays sharp when the editor is scaled.

## Regenerating

From the repository root:

```
python -I tools/brand/make_brand.py
```

Then rebuild. The script needs Pillow. It writes into `modules/suite_ui/brand/`. Do not edit the PNGs by hand.

## Known limits

- The lockups are bitmaps, so they do not scale past their rendered size. For a larger display, render at a higher `SCALE` in the script.
- Title rows are 40 px in SpectralDrift and DynamicSaturation, and 60 px in Micro-Timing Chaos. The lockup is drawn to fit the row's height.
