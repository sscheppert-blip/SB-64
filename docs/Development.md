# SB-64 – Development

## Building

Needs CMake ≥ 3.22, a C++20 compiler and Ninja (optional). JUCE and clap-juce-extensions are
fetched by CMake.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/SB64Tests_artefacts/Release/SB64Tests
```

The plugins are copied into the user plugin folders after building (on Windows the VST3 goes to
`Common Files\VST3`), and on macOS the Standalone app into `~/Applications` (`-DSB64_COPY_PLUGIN=OFF`
to skip). The CLAP file stays in `build/SB64_artefacts/Release/CLAP`.

The test runner also has a few tools:

- `--render <wav> [option…]` plays a short acid riff (16ths at 125 BPM, with accents and slides) and
  writes it as a mono WAV. Options: `<paramId>=<value>` (plain values, e.g. `cutoff=0.3 wave=0
  filterMode=0`), `seconds=<n>`, `note=<n>` to hold one note instead of the riff.
- `--snapshot <png> [scale]` renders the panel (design size 1580 × 960 at scale 1).
- `--icon <png>` renders the app icon (`design/icon.png`, 1024 px, used by the Standalone app).
- `--bench` measures the CPU load.

## Releases

Builds run only on demand (pushes don't trigger them, to save GitHub Actions minutes). To publish a
new version, bump `VERSION` in `CMakeLists.txt`, commit and push, then:

- **On a Mac:** `tools/release-mac.sh 0.1.0` builds, tests and validates the macOS plugins locally as
  universal binaries, creates a draft release with them, then lets GitHub build only Linux and
  Windows and publish. `--dry-run` stops after packaging.
- **On a PC:** `gh workflow run build -f version=0.1.0` (or Actions → build → Run workflow) builds
  all three platforms on GitHub.

## Validation (macOS)

```bash
auval -v aumu Sb64 Sbli
pluginval --strictness-level 10 --validate-in-process --validate ~/Library/Audio/Plug-Ins/VST3/SB-64.vst3
clap-validator validate ~/Library/Audio/Plug-Ins/CLAP/SB-64.clap
```

`tools/check-sensitive-data.sh` (needs ripgrep) scans the tracked files for credentials and local paths.

## Fonts

Michroma (labels) and DSEG14 Classic (segment display) are embedded from `design/fonts/`, both under
the SIL Open Font License (`design/fonts/OFL-*.txt`).
