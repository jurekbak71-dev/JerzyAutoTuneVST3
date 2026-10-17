# JERZY AUTO TUNE 2.0 — Advanced Vocal Processor

VST3 vocal pitch correction and processing, developed from version 1.5.0 (`48207fd`) with an independent Advanced DSP engine. The target is Windows 10/11 x64, FL Studio and other VST3 hosts. Native Windows/FL Studio qualification remains required before a production release; Linux regression results do not establish commercial-product equivalence.

## Processing

Pitch correction → Gate → Noise Filter → De-Esser → Saturation → Doubler → Compressor → Vocal EQ → Output → Final Limiter

- Sample-rate-aware monophonic YIN pitch tracking, 50–1200 Hz, confidence and voiced/unvoiced gating, harmonic tests and persistent note transitions.
- Key, scale and 12-note mask; Speed, Amount and Mix; new Vibrato Preserve, Note Stability, A4 reference and independent Transpose controls.
- Explicit spectral-envelope compensation and independent Formant Shift using the existing pinned Signalsmith Stretch library. Correction controls are aligned to the spectral processing clock.
- Fractional interpolated doubler delay; the original detune control remains nominal modulation depth rather than a constant-cent shift.
- Smooth 10 ms module bypasses with continuously updated states in Advanced mode. Output and formant changes are smoothed.
- Independent stereo-linked 3 ms lookahead limiter **after** EQ and output trim. It bounds sample peaks, not intersample/true peaks. Limiter bypass keeps host latency constant.
- Vector hardware interface, all 68 automated parameters, typed values, fine adjustment, zoom, remembered size, input/output meters, compressor reduction, detected/target note and confidence.

The Noise Filter retains the original expander/high-pass design; it is not a learned spectral denoiser. This release does not implement graphical offline note editing, MIDI-controlled tuning or polyphonic processing.

## Existing sessions

The original 60 parameter IDs, order, ranges and defaults, APVTS state type and VST3 identity (`JzAu` / `JzB4`, `audio.jerzy.autotune.v14`) are retained. Eight parameters are appended. Old states automatically select **Legacy 1.5**, retaining the original detector, doubler and soft clipper. Choose **Advanced** to use the new DSP. Legacy mode deliberately retains its old DSP limitations.

The final 3 ms delay is reported to the host in both modes. Window height is clamped to the new minimum of 720 pixels. Check real project recall and PDC in FL Studio before replacing an installed production version. Engine selection is best made before playback.

## Windows x64 build and tests

Requires CMake 3.24+, Visual Studio 2022 with C++ desktop tools, Git and Internet access for the pinned JUCE 8.0.6 and Signalsmith dependencies.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target JerzyAutoTune_VST3 JerzyAutoTuneTests JerzyVocalDSPTests --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

The plugin bundle is `build/JerzyAutoTune_artefacts/Release/VST3/JERZY AUTO TUNE.vst3`. The Windows workflow builds all three targets, runs both suites, verifies module metadata and packages the bundle. Updating the workflow here does not mean that a GitHub Actions run has executed.

## Cloud Linux development

This task's local toolchain and JUCE headers/libraries live outside the checkout under `/workspace/.jerzy-env`. No system administrator access is required. Installation/start instructions are saved separately in the cloud environment configuration.

```bash
source /workspace/.jerzy-env/activate.sh
cd /workspace/JerzyAutoTuneVST3
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_CXX_STANDARD_LIBRARIES=-flto=2
cmake --build build --target JerzyAutoTune_VST3 JerzyAutoTuneTests JerzyVocalDSPTests --parallel 4
xvfb-run -a ctest --test-dir build --output-on-failure
```

The `-flto=2` override is specific to the cloud GCC toolchain and limits link-time parallelism. Do not pass it to MSVC. GUI snapshots are generated in `build/gui-previews`.

See [baseline audit](docs/AUDIT-1.5.md), [DSP design and limits](docs/DSP-2.0.md) and [validation record](docs/VALIDATION.md).
