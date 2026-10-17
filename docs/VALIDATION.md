# Validation record — 2.0.0

Machine: Debian 13 Linux x64, GCC 14.2, CMake 3.31.6, Ninja 1.12.1; JUCE 8.0.6 and the original pinned Signalsmith Stretch revision. Builds use Release with four compilation jobs and two LTO workers per link. Dependencies were downloaded using signed Debian repository metadata and extracted into `/workspace/.jerzy-env`; neither signature nor TLS verification was disabled.

## Baseline

Unmodified main at 48207fdcd3845c83ad9a1c29f9cad38faced3dcc built both the Linux VST3 and regression executable. Original CTest: 1/1 passed (0.83 s). See AUDIT-1.5.md for the findings.

## Automated coverage

- `vocal_dsp_regressions`: 95 cases across 44.1/48/88.2/96/192 kHz, including 55–1100 Hz sinusoidal and missing-fundamental harmonic signals, out-of-range tones, noise/DC rejection, stereo-linked sample ceiling, audible output and exact limiter delay.
- `autotune_gui_and_parameters`: all 68 controls in both automation directions; small/large/non-proportional scaling, transformed hit targets, snapshots, editor lifecycle and state recall. The original 60 IDs remain an exact prefix; state without engineSchema must select Legacy.
- Measured 447 Hz → 440 Hz output correction at 44.1/48/96/192 kHz, including opposite-polarity stereo; measured dry impulse latency and a final ceiling after simultaneous EQ/output boosts with compressor bypassed.
- Output expression test: 5 Hz vibrato of ±22 cents around 447 Hz, comparing full correction and 100% Vibrato Preserve through the actual plugin.
- Output formant test: a synthetic harmonic vowel, measuring a spectral-centroid change between −5 and +5 semitones of Formant Shift.
- Bypass continuity and eventual dry output for gate, noise filter, saturation, doubler, compressor and EQ. De-esser is covered by control/state wiring and shared ramp processing, not a dedicated sibilance suppression benchmark.
- Dry-path latency with mono/stereo and blocks of 1, 17, 64 and 1024 samples, including blocks larger than the prepared estimate. This is not a proof of identical pitch trajectories across every host block arrangement.
- Standalone DSP suite also passed AddressSanitizer and UndefinedBehaviorSanitizer, 95 cases.
- Source comparison confirms the original parameter definitions and VST3 identity block are byte-identical to the baseline.

Final 2.0.0 Release build: Linux VST3, JerzyAutoTuneTests and JerzyVocalDSPTests all built successfully. CTest: **2/2 passed**, 5.80 seconds. Both suites executed; no checks were disabled to obtain this result. The additional state roundtrip, Legacy migration and mono/variable-block assertions executed in this final run.

Measured output vibrato standard deviation was 2.01568 cents with full correction and 15.376 cents with 100% preservation. The synthetic vowel spectral centroid moved from 945.288 Hz at −5 semitones to 1511.26 Hz at +5 semitones. These are results for the stated synthetic inputs, not a general perceptual quality score.

Generated VST3 moduleinfo reports version 2.0.0 with the existing component/controller class IDs. The native module-info helper loaded the Linux VST3 to generate metadata. This does not qualify Windows or FL Studio.

## Required target-platform qualification

Windows 10/11 x64 and FL Studio are unavailable in this Linux machine. The updated Windows Actions workflow has not been run from this task. Remaining checks are native compilation and VST3 loading, existing 1.5 project recall, host bypass and PDC, live automation and playback restarts, offline export, multiple instances, DPI 100/125/150%, and listening/quality assessment on real vocal recordings. No comparison establishing parity with a commercial processor has been performed.

The limiter bounds sample peaks, not true peaks. The engine retains a substantial spectral-processing latency; no low-latency monitoring mode is claimed. Noise filtering remains an expander/HPF. The implementation is a release candidate for native qualification.

## Reusable environment configuration

The final saved install_script was executed end to end again and completed successfully, including rebuilding the targets and passing both CTest suites. Dependency extraction is cached using SHA-256 archive stamps. This establishes repeatability on the current machine; restoration in a newly published task has not been tested.

Both install_script and start_skill were saved successfully in the environment configuration draft. Publication remains a user action in environment settings; this task has not published the snapshot. The cloud setup does not itself publish a GitHub release.
