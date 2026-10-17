# Advanced engine design and limits

This is an independent implementation using published DSP methods and the existing permissively licensed Signalsmith Stretch dependency. No Antares code, visual assets, brand elements or DVA implementation are used.

## Detection and note selection

The advanced tracker uses the cumulative mean normalised difference from YIN (de Cheveigne and Kawahara, JASA 2002), with parabolic refinement of local minima. Its time/frequency bounds follow sample rate: 50–1200 Hz, approximately 60 ms of history and a 5 ms analysis hop. A fourth-order Butterworth low-pass precedes integer decimation to approximately 12–15 kHz. Mean-subtracted energy gating rejects silence/DC; periodicity confidence rejects aperiodic frames. A candidate more than one semitone away must persist for two analysis frames. This is a monophonic voiced/unvoiced heuristic, not phoneme recognition or polyphonic transcription.

Correction control is delayed to align the centre of the detector observation with the spectral processing clock (including the centre of the split-computation interval). This avoids adding phase error to vibrato correction.

Pitch correction targets the intersection of key/scale and the 12-note mask. An empty intersection bypasses correction. Stability provides target-note hysteresis. Vibrato Preserve blends instantaneous pitch with a slowly tracked note centre when calculating correction, retaining fast expressive variation at higher settings. Reference A4 and independent semitone transposition are additional controls. Speed retains its existing units, with a 2 ms minimum smoothing time. There is no claim of zero-latency pitch detection.

## Spectral processing and dynamics

Signalsmith Stretch is retained at the original pinned commit. Its spectral-envelope compensation and independent formant-shift API are used explicitly. The tracked fundamental is supplied in normalised frequency units as documented by Signalsmith. Formant compensation is optional. This is a spectral model, not a full vocal-tract physical model.

The advanced doubler reads fractional delay positions with linear interpolation. The original nominal detune-depth control is retained for session compatibility; it does not become a constant-cent pitch shifter.

All seven module bypasses crossfade over 10 ms in Advanced mode, while DSP state continues to update. Output trim ramps over 20 ms and formant shift over 30 ms. Filter coefficient smoothing is retained. This does not imply sample-accurate smoothing of every continuous parameter.

An independent final limiter follows EQ and output trim, even when the compressor is bypassed. A stereo-linked sliding-window peak detector controls a 3 ms delay, with instantaneous safe attenuation and 80 ms release. Storage is allocated during prepareToPlay, with no per-sample queue allocations. It guarantees the sample-peak bound while enabled; it is **not an oversampled true-peak limiter** and does not guarantee intersample peaks. Its delay remains in place when bypassed, so limiter automation does not change host latency. Host latency includes both the existing pitch engine latency and this delay.

## Compatibility

The original 60 parameter IDs, order, ranges/defaults, APVTS PARAMETERS tree and VST3 identity remain intact. Eight parameters are appended. State now carries engineSchema=2. State without that property selects Legacy 1.5 and explicit defaults for the appended parameters. Legacy retains the original pitch detector, integer doubler and pre-EQ soft clipper. The final delay remains in the path and is reported to the host; therefore old and new rendered files need latency alignment when comparing. A successful XML/state regression is not proof that every existing FL Studio project recalls correctly: native host testing is still required.

## Release qualification

Synthetic tests are deterministic regression evidence, not an auditory comparison with commercial products. Windows 10/11 x64 builds, FL Studio loading/recall, offline export, host bypass/PDC, DPI scaling and listening tests on consented vocal recordings must be checked on the target platform. No Windows or FL Studio result should be inferred from Linux tests. Noise Filter remains the existing expander plus high-pass, not a learned spectral denoiser. Graphical offline note editing, MIDI-controlled tuning and polyphonic processing are not implemented.
