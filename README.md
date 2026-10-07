# JERZY AUTO TUNE 1.4

Standalone Windows x64 VST3 vocal pitch-correction and vocal-processing plug-in for FL Studio and other VST3 hosts.

## Signal chain

Pitch Correction → Gate → Noise Filter → De-Esser → Saturation → Doubler → Vocal Compressor + Limiter → Vocal EQ → Output

## Pitch correction

- Key and Scale selectors
- 12-note filter: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
- Speed
- Amount
- Mix

## Vocal processing controls

- Gate: Threshold, Range, Attack, Hold, Release
- Noise Filter: Threshold, Reduction, Release, High-Pass
- De-Esser: Threshold, Frequency, Range, Release
- Saturation: Drive, Tone, Mix, Output
- Doubler: Mix, Delay, Detune, Width
- Compressor + Limiter: Threshold, Ratio, Attack, Release, Knee, Makeup, Limiter Ceiling
- Vocal EQ: Low Cut, Body, Mid Frequency, Presence, Mid Q, Air, High Cut, Output

All exposed controls are VST3 parameters and can be automated by the host.

## Build

Requires CMake and Visual Studio on Windows. Dependencies are fetched automatically.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target JerzyAutoTune_VST3
```

The GitHub Actions workflow builds, verifies and packages the Windows x64 VST3 artifact automatically.


<!-- Jerzy VST GUI System CI validation -->

<!-- Jerzy GUI validation pass 2 -->
