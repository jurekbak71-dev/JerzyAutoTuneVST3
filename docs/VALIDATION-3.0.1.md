# Steel Studio 3.0.1 RC2 validation — 2026-10-30

Source: `8ddfbedabd60e1243e88361e15208ff5a3d40b73`.

Windows x64 / MSVC workflow completed successfully:
https://github.com/jurekbak71-dev/JerzyAutoTuneVST3/actions/runs/41051655932

Passed native compilation, both CTest suites (143 parameter bindings, caption size/width checks, GUI resize/state/lifecycle, 95 pitch/limiter cases and 20 studio audio cases), module version/PE x64 verification and pluginval 1.0.4 strictness 5. The installer test first installed RC1, upgraded it to 3.0.1, checked the installed module version, validated the installed plugin and uninstalled it with binary-removal verification.

The release contains the actual installer, SHA256, a native Windows Tune screenshot and a ZIP with 14 native GUI captures. The screenshots are driven by an actual synthetic audio test signal through the processor. The published installer was downloaded and its SHA256 rechecked locally.

Release: https://github.com/jurekbak71-dev/JerzyAutoTuneVST3/releases/tag/v3.0.1-rc2

Installer SHA256: `74699b84800dd96a130431f47323e7941c3ff207df1609fe0abe365f48472be2`.

Native Windows screenshots were visually reviewed after publication. The implementation uses embedded Barlow fonts and original metal texture, with Feather icons; licences are included in the installer notices. See ../design-qa.md for visual comparison and explicit P3 differences from the selected reference.

Remaining qualification: manual FL Studio listening/project recall and real Windows 10/11 high-DPI interaction. The Windows Server runner checks do not establish those results. This is an unsigned release candidate.
