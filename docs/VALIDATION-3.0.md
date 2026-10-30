# Vocal Studio 3.0 RC1 validation

Validated source: `f4e6c6c8280b2fe040cbc02ec7c1dc77e0fee665` on `codex/vst3-2.0`.

Native Windows 2022 / MSVC x64 workflow completed successfully on 2026-10-30:
https://github.com/jurekbak71-dev/JerzyAutoTuneVST3/actions/runs/41047913203

Passed:

- Native VST3 and regression test compilation; Windows PE x64 and module version verification.
- Both CTest suites, including all 143 parameter bindings and GUI sizes/state/lifecycle, 95 pitch/limiter regressions and 19 studio audio regressions. Local Linux/Xvfb suites also passed.
- pluginval 1.0.4, strictness 5, deterministic seed 71, built VST3 and installed VST3. Optional external Steinberg validator was not configured; pluginval's auval section is not a macOS certification.
- Inno Setup build, silent installation to standard shared VST3 location, installed plugin validation, and uninstallation with plugin binary removal.
- Release candidate published only after the above steps succeeded.

Installer: https://github.com/jurekbak71-dev/JerzyAutoTuneVST3/releases/download/v3.0.0-rc1/JerzyVocalStudio-3.0.0-Windows-x64-Setup.exe

SHA256: `1a43e6016ca7005675ac35818cb7aeb8af0586175c5d2a73d7f00375f19fb1f7`.

Limitations: no code-signing certificate; manual FL Studio listening, saved-project recall, Windows 10/11 desktop/high-DPI interaction and subjective vocal quality qualification remain unperformed. Windows Server 2022 runner tests do not establish these outcomes. This is a release candidate, not a claim of commercial-product equivalence.
