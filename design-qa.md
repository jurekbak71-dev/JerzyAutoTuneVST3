# Steel Studio native design QA — 3.0.1 RC2

Reference: the user's selected Steel Studio contact sheet, original image `exec-1447ef30-6f91-4c12-a219-f8a1050ee605.png`. Compare the application area, excluding the contact sheet's title and section captions.

Implementation: native JUCE 1280×800 captures in `build/gui-previews`, seven focus screens and seven expanded screens. These are real component snapshots driven by a synthetic monophonic audio signal through the actual processor. No pitch traces, spectra or meters are painted from invented demo data. The two lower Tune histories display measured input/preparation amplitude envelopes, explicitly labelled IN/PREP.

Resolved findings:

- P1: replaced the flat UI shell with layered dirty-silver metal borders and inset oxidized black panels. Added library icons to fixed left navigation and a compact toolbar with actual preset, A/B, undo, latency, zoom and power controls.
- P1: restored the Tune hierarchy: large pitch graph, separate current-note/cents card, five large inspector controls, lower audio histories, note mask and fixed master meters.
- P1: replaced host-dependent fonts with embedded Barlow Condensed SemiBold labels and Barlow SemiBold numeric fields. Parameter captions are at least 17 design pixels and numeric fields 18; chart text is at least 14. Caption-width tests fail on truncation.
- P2: primary Clean/Tone/Space/Voice controls are large and spaced; less-used parameters remain accessible in ALL CONTROLS. Harmony uses compact enabled switches, level/interval sliders and readable pan/formant controls rather than microscopic knobs.
- P2: indicators retain green ON/red OFF text and LEDs, red zero ticks and genuine overload holds. Normal signal plots and meters use neutral silver.
- P2: real dBFS meter scale, time/frequency axes, rounded cents/key-confidence fields, correct EQ gain labels, and a compression curve sharing the DSP's knee function. A newly identified exact-boundary soft-knee discontinuity is corrected and regression-tested.

Remaining P3 differences: the fonts approximate the reference's condensed lettering; the typography and material rendering are not pixel-identical to the generated contact sheet. Space presents the implemented decay target rather than inventing a measured impulse response. Harmony interval sliders retain the existing host parameter type. The master strip additionally exposes the actual limiter ceiling and bypass. These are explicit functional adaptations.

Validation: native Linux and Windows GUI/audio tests passed, including caption size/width checks. Windows x64 build, pluginval, RC1 upgrade and installer checks passed in run 41051655932. The release screenshots were downloaded and the Windows Tune capture was visually reviewed. See docs/VALIDATION-3.0.1.md. Manual FL Studio listening/high-DPI qualification remains distinct from visual implementation.

final result: passed
