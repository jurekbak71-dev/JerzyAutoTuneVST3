# FL Studio integration — development branch v0.1
## Harmonic workflow
Generate Bass Ambient harmony guide with `jerzybridge bass` or build chords through Piano Roll. Copy key/scale manually to Auto Tune; external chord MIDI is NOT interpreted by current Auto Tune DSP.
- Set Key and Scale or 12-note mask in Auto Tune before recording.
- For modulations use native FL Studio Automation Clips (automate plugin parameters), verify switching mask boundary click-free.
- MiniFreak V or Mono Synth Grid can act as harmonic guide track.
## Regression / optimization
At 44.1/48/96 kHz test low/high notes, note attack/release, CPU, MIDI transport discontinuities, detector window limits, click-free bypass and final limiter ceiling.
## Current branch
Integration prescription only; DSP improvements remain unimplemented and require audio tests.