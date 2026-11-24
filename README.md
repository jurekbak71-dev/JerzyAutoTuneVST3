# Jerzy Vocal Studio 3.0.1 / JERZY AUTO TUNE VST3

Native Windows 10/11 x64 vocal processor, extended from JerzyAutoTuneVST3 rather than replaced. The host-visible name remains **JERZY AUTO TUNE** and the original VST identity and 68 existing parameter IDs are retained. The Steel Studio interface exposes 143 automated parameters across Tune, Clean, Dynamics, Tone, Space, Harmony and Voice.

## Processing and interface

- Sample-rate-aware YIN pitch tracking, confidence, note transitions, scale/note mask, vibrato preservation, independent transpose and spectral-envelope formant compensation using pinned Signalsmith Stretch.
- Learned spectral noise reduction: learn approximately one second of isolated noise, then adjust reduction and audition the removed signal. Noise profiles are saved with the session and invalidated at a different sample rate.
- Two independent stereo-linked compressors with Clean, Fast FET and two optical-style envelope responses, sidechain high-pass and Warm shaping. Soft and transient hard-sibilance controls.
- Existing vocal EQ with draggable response nodes, saturation models and fractional-delay doubler.
- Algorithmic room, plate-style and hall reverberation; tempo-synced or free-time ping-pong delay; wet ducking.
- Four independently configured harmony voices, interval/scale/MIDI modes, per-voice gain, pan and formant. Fractional-delay choir with 4/8/16/32 unison voices.
- Spectral breath/character shaping; key suggestion from confident monophonic phrases with explicit Apply.
- Oxidized steel, black and dirty silver native vector interface. Red zero ticks and overload indicators; green ON / red OFF LEDs with text. Focus view and ALL CONTROLS, typed values, zoom, session size recall, factory presets, A/B and undo.
- Final stereo-linked 4x FIR intersample-aware lookahead limiter after effects and output trim. Latency-compensated master bypass.

This is a **Studio-latency** processor; the host receives the actual latency. It does not promise low-latency live monitoring. The final limiter now detects intersample peaks with a 4x polyphase FIR and reserves approximately 0.7 dB of limiting guardband. This is a finite-rate true-peak *estimate*, not a certified BS.1770 meter or an unconditional continuous-time peak guarantee. Learned noise reduction and voice shaping are independently implemented DSP, not trained AI or a complete physical vocal-tract simulation. Key suggestion does not analyze polyphonic files or infer BPM; delay reads host tempo. Manual offline note editing is not implemented. Commercial-product equivalence is not asserted.

## DSP repairs under review

The `fix/autotune-301-dsp-audit-20261124` branch adds first-order antiderivative antialiasing (ADAA) for the two tanh saturation models, with a per-channel state and stable log-cosh evaluation. This attenuates folded high-frequency harmonics; it has a small effective fractional-sample phase delay and is **not** identical to the legacy saturation sound. Input/output spectral plots and optional Windows CI alias tests are available for comparisons.

The 4x FIR intersample-aware output limiter retains the original 3-ms audio delay and the existing VST3 latency declaration. Its sample-peak and adversarial intersample test cases must pass on the native Windows CI before merge; test coverage cannot establish a mathematical guarantee for all future waveforms.

Once the master bypass crossfade reaches fully dry, the plugin now has a CPU-saving path that maintains the latency-aligned dry ring, peak meters and audio visualizers while skipping the expensive pitch and effects graph. MIDI Note On/Off, sustain pedal and all-notes-off events are still processed by a lightweight note latch while the audio DSP sleeps; waking cannot resurrect notes already released during bypass. Re-enabling processing primes the graph from incoming audio before fading wet back in. This is intended to prevent a transient dropout, but real FL Studio testing is required before release.

No VST3 parameter IDs, factory program identities or plugin IDs are changed. Do not merge the Draft PR or publish a production release solely because one CI run passes.

## Native visualizers and honest signal paths

- Tune history is a **measured** 30 Hz pitch/target timeline (about 8.7 seconds); it holds position when no new audio arrives. The pitch viewport pans slowly, not on every note transition.
- Clean/Voice spectra compare the **latency-aligned dry input** and **actual final VST3 output after the complete chain and global bypass**, sampled over a 2048-sample Hann FFT window. The curves are smoothed for readability; no generated spectrum is presented as a measurement.
- Tone shows the theoretical **EQ transfer function** on a separate ±18 dB gain scale. It is intentionally not overlaid on an incompatible 0 to -72 dB spectrum scale.
- Dynamics shows the **modelled** compressor transfer functions separately from **measured** gain-reduction histories for each stage. The histories only advance when audio does.
- Space displays a **modelled decay envelope**, not an impulse-response measurement. The caption states when the reverb is disabled.
- Input/Output dBFS meters aggregate peaks across all host callbacks between GUI timer frames; overload latches until reset. The GUI meters remain **sample-peak** (not EBU/R128 loudness meters or true-peak meters), even though the final limiter uses 4x intersample detection.

Native Windows screenshots in CI may use synthetic audio for deterministic regression. Such screenshots demonstrate the real renderer and DSP paths, but are not substitutes for live FL Studio measurements, listening and DPI qualification.

## Install

Use the Windows x64 Setup executable produced by the Windows workflow. Close the DAW before installing; the bundle goes to `C:\Program Files\Common Files\VST3\JERZY AUTO TUNE.vst3`. Rescan plugins in FL Studio. Back up existing projects before replacing an older installed version. Legacy states select the original legacy pitch engine; new effect parameters receive safe defaults. Real FL Studio project recall and listening remain manual qualification tasks.

## Build and validation

Windows: Visual Studio 2022 C++ tools, CMake 3.24+, Git. Dependencies: JUCE 8.0.6 and Signalsmith Stretch pinned in CMake.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target JerzyAutoTune_VST3 JerzyAutoTuneTests JerzyVocalDSPTests --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

The Windows workflow additionally validates the VST3 using pluginval 1.0.4 strictness 5, builds an Inno Setup installer, installs it, validates the installed plugin, then uninstalls it. Artifacts include installer, SHA256 and validation logs. A workflow definition alone is not evidence of a successful run.

Linux cloud development:

```bash
source /workspace/.jerzy-env/activate.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DJERZY_ENABLE_LTO=OFF -DCMAKE_CXX_STANDARD_LIBRARIES=
cmake --build build --target JerzyAutoTune_VST3 JerzyAutoTuneTests JerzyVocalDSPTests -j4
xvfb-run -a ctest --test-dir build --output-on-failure
```

Regression coverage includes 95 detector/limiter cases, 20 studio audio cases, all 143 parameter bindings, scaling, hit targets, state recall and native section screenshots. See [research](docs/RESEARCH-3.0.md), [original audit](docs/AUDIT-1.5.md), [pitch DSP](docs/DSP-2.0.md) and [third-party notices](docs/THIRD-PARTY-NOTICES.md).

## Steel Studio GUI update (3.0.1 RC2)

The native interface now follows the selected hardware layout: multi-layer metal frame, icon navigation, separate current-note card, large inspector knobs, input/processed amplitude histories, readable dBFS meters and larger primary effect controls. Barlow and Barlow Condensed fonts are embedded for consistent rendering on Windows. All parameter bindings and DSP remain intact. Advanced settings remain in ALL CONTROLS. Native screenshots in the release are driven by a synthetic audio test signal.
