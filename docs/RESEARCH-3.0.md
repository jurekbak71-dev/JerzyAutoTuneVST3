# Research and implementation boundaries

The eleven additional Jerzy repositories were inspected as references for native JUCE structure, DSP state management, testing and Windows delivery. In particular, JerzyOilDrumEcho supplies a useful example of delay-network testing and native validation; JerzyTapeDriveVST3 illustrates separating nonlinear and envelope stages; JerzyFLPluginCleaner documents Windows plugin locations. Their source was not copied into this processor.

External primary sources consulted:

- https://github.com/Signalsmith-Audio/signalsmith-stretch — MIT library already present in the project; pinned implementation retained for lead and four parallel harmony engines, including spectral-envelope controls.
- https://github.com/xiph/rnnoise — BSD-3-Clause learned voice denoising reference; not integrated. The implemented denoiser instead learns a stationary noise power spectrum and uses bounded smoothed spectral gains, without neural-model claims.
- https://github.com/Chowdhury-DSP/chowdsp_utils — reference ecosystem for audio utility design; no source incorporated and no licence inferred from an unresolved repository metadata field.
- https://github.com/Tracktion/pluginval/releases/tag/v1.0.4 — pinned independent VST3 host validator, strictness 5 with reproducible seed 71.
- https://github.com/jrsoftware/issrc — native Inno Setup installer, using the Windows runner's installed compiler.
- https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md — Visual Studio, CMake and Inno Setup availability checked for Windows 2022 delivery.

Independent processing follows established methods: YIN cumulative mean normalized difference for F0; square-root Hann overlap-add for spectral noise processing; stereo-linked feed-forward compression with soft knee; fractional delay interpolation for unison modulation; Householder feedback diffusion with frequency damping for reverberation; Krumhansl-Schmuckler tonal-profile correlation for monophonic key suggestions. The last method needs a representative phrase and can confuse relative major/minor; Apply remains explicit.

Antares material is a functional and ergonomic reference only. No Antares code, DVA algorithm, trademarked visual identity, photographs or UI assets are incorporated. The oxidized steel asset is original generated artwork. This implementation does not assert patent clearance or product parity based on marketing descriptions.

Validation is split between deterministic DSP/GUI regressions, native Windows host validation, installer lifecycle checks, and manual listening/DAW checks. Automated tests cannot establish subjective vocal quality or prove compatibility with every saved FL Studio project.
