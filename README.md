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
- Final stereo-linked lookahead sample-peak limiter after effects and output trim. Latency-compensated master bypass.

This is a **Studio-latency** processor; the host receives the actual latency. It does not promise low-latency live monitoring. The limiter bounds sample peaks, not intersample true peaks. Learned noise reduction and voice shaping are independently implemented DSP, not trained AI or a complete physical vocal-tract simulation. Key suggestion does not analyze polyphonic files or infer BPM; delay reads host tempo. Manual offline note editing is not implemented. Commercial-product equivalence is not asserted.

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
