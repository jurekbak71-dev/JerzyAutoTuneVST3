# Native GUI review — 2026-10-30

The selected Steel Studio reference is implemented as native JUCE, not a web view. Default size 1280×800, resizable with centered scaling; seven persistent section buttons, right master strip, focus/expanded control views. Original generated oxide texture is embedded in the binary. No runtime external image dependencies.

Local native screenshots reviewed: Tune focus and expanded section captures under `build/gui-previews`. Red zero marks occur at the actual parameter zero where zero is within range. Toggle LEDs include ON/OFF labels; overload uses red, ordinary meter and spectrum activity use neutral colours. Space graph uses seconds and dB rather than an unrelated frequency axis.

Automated GUI checks exercise all 143 parameters through the visible-page union, host/control binding, multiple sizes including 960×600, transformed hit targets, zoom, state roundtrip and repeated editor lifecycles. Screenshots are regenerated from the native component tree and written by truncating existing image files.

This is a functional interpretation of the chosen material palette and layout, not a pixel-identical reproduction of a marketing mockup. Blank pitch history before playback and absent latency before prepareToPlay are intentional. Native Windows captures and test logs are separately published by the build workflow. Manual Windows high-DPI, keyboard-only operation and FL Studio project recall remain qualification work; local Xvfb screenshots do not establish these outcomes.
