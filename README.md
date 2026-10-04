# JERZY AUTO TUNE 1.5

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

## 1.5 GUI update

- Default 1008 x 702 window; vector zoom 75-150% (minimum 840 x 585).
- A single transformed surface keeps panels, fonts, text entry and mouse targets aligned.
- All 60 host parameters remain accessible. The seven vocal processors are always visible;
  compressor and EQ use two rows. Output trim is separate and stays active with EQ bypassed.
- Window size is restored on reopen and saved in host state; existing parameter IDs and the
  VST3 identity are unchanged, so 1.4 sessions remain compatible.
- Editable values with units, double-click reset, fine adjustment with Shift, tooltips,
  explicit module bypass indication, input/output dBFS meters and compressor gain reduction.
- The note filter indicates the effective scale/mask intersection. ALL and NONE edit the mask.
  An empty intersection now bypasses pitch correction instead of selecting a forbidden note.
- Actual editor snapshots and tests cover all parameters, automation, small/large/non-proportional
  windows, transformed mouse targets, reopen/restore, finite audio, note-mask handling and stereo metering.

## Remaining DSP improvements identified during review

These are not changes to the sound engine in the GUI update:

1. The current limiter is a `tanh` soft clipper before EQ/output. It does not guarantee the final
   output ceiling. A dedicated final limiter with oversampling/lookahead needs separate audio tests.
2. Doubler uses integer delay taps. Fractional interpolation would reduce modulation stepping;
   the detune label is nominal modulation depth rather than calibrated constant cents.
3. Pitch detection uses fixed sample lags (44-850). At 96 kHz it cannot cover the same low vocal
   range as at 44.1/48 kHz. Scale lag limits and analysis windows with sample rate.
4. Bypassing processors currently switches processing immediately. Short crossfades and parameter
   smoothing are desirable for click-free live automation.
5. The noise filter is an expander plus high-pass filter, not a learned spectral denoiser.

A successful CI build and native editor tests do not replace testing the VST3 in FL Studio,
including Windows display scaling at 100%, 125% and 150%.
