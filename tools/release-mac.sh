#!/usr/bin/env bash
# Release from a Mac. The macOS packages are built and validated here (free), then GitHub
# Actions builds only Linux + Windows, adds them to the release and publishes it.
#
#   tools/release-mac.sh 0.5.0             build, draft release, start Linux/Windows, publish
#   tools/release-mac.sh 0.5.0 --dry-run   build, test and package only; nothing goes to GitHub
#
# Needs: Xcode command line tools, cmake, ninja, gh (logged in), ripgrep.
set -euo pipefail

die() { echo "release-mac: $*" >&2; exit 1; }

version="${1:-}"
dry_run=false
[[ "${2:-}" == "--dry-run" ]] && dry_run=true
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "usage: tools/release-mac.sh <version, e.g. 0.5.0>"
[[ "$(uname)" == "Darwin" ]] || die "macOS only; on other systems run: gh workflow run build -f version=$version"
tag="v$version"

cd "$(git rev-parse --show-toplevel)"

# ── preconditions ───────────────────────────────────────────────────────────
git fetch -q origin
[[ -z "$(git status --porcelain)" ]] || die "uncommitted changes – commit or stash first"
[[ "$(git rev-parse --abbrev-ref HEAD)" == "main" ]] || die "switch to main first"
if [[ "$dry_run" == false ]]; then
    [[ "$(git rev-parse HEAD)" == "$(git rev-parse origin/main)" ]] || die "local main differs from origin/main – push or pull first"
    gh release view "$tag" >/dev/null 2>&1 && die "release $tag already exists"
fi
grep -Eq "^project\(SB64 VERSION ${version//./\\.} " CMakeLists.txt \
    || die "version $version does not match VERSION in CMakeLists.txt – bump it, commit and push first"
./tools/check-sensitive-data.sh

sha=$(git rev-parse HEAD)
build=build-release
release=$build/SB64_artefacts/Release
dist=$build/dist

# ── build + test (universal binary) ─────────────────────────────────────────
echo "▸ building macOS packages for $tag ($sha)"
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DSB64_COPY_PLUGIN=OFF >/dev/null
cmake --build "$build" --target SB64_AU SB64_VST3 SB64_CLAP SB64_Standalone SB64Tests
"$build/SB64Tests_artefacts/Release/SB64Tests"

for bundle in VST3/SB-64.vst3 CLAP/SB-64.clap AU/SB-64.component Standalone/SB-64.app; do
    codesign -v "$release/$bundle"
    [[ "$(lipo -archs "$release/$bundle/Contents/MacOS/SB-64")" == *arm64* ]] || die "$bundle is not universal"
    [[ "$(lipo -archs "$release/$bundle/Contents/MacOS/SB-64")" == *x86_64* ]] || die "$bundle is not universal"
done

# auval needs the component installed; this replaces any installed SB-64.component.
echo "▸ validating AU"
mkdir -p ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/Components/SB-64.component
cp -R "$release/AU/SB-64.component" ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aumu Sb64 Sbli | tail -3

# Further validators (see docs/Development.md), when installed.
if command -v pluginval >/dev/null 2>&1; then
    echo "▸ pluginval"
    pluginval --strictness-level 10 --validate-in-process --validate "$release/VST3/SB-64.vst3" | tail -2
fi
if command -v clap-validator >/dev/null 2>&1; then
    echo "▸ clap-validator"
    clap-validator validate "$release/CLAP/SB-64.clap" | tail -2
fi

# ── package, draft release, start the Linux/Windows build ───────────────────
rm -rf "$dist" && mkdir -p "$dist"
ditto -c -k --keepParent "$release/VST3/SB-64.vst3"      "$dist/SB-64-macOS-VST3.zip"
ditto -c -k --keepParent "$release/CLAP/SB-64.clap"      "$dist/SB-64-macOS-CLAP.zip"
ditto -c -k --keepParent "$release/AU/SB-64.component"   "$dist/SB-64-macOS-AU.zip"
ditto -c -k --keepParent "$release/Standalone/SB-64.app" "$dist/SB-64-macOS-Standalone.zip"

if [[ "$dry_run" == true ]]; then
    echo "✓ dry run done – packages in $dist:"
    ls -lh "$dist" | awk 'NR > 1 { print "  " $5 "  " $9 }'
    exit 0
fi

echo "▸ creating draft release $tag with the macOS packages"
gh release create "$tag" "$dist"/* --draft --target "$sha" --title "SB-64 $version" --generate-notes >/dev/null

echo "▸ starting the Linux + Windows build on GitHub"
started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
gh workflow run build --ref main -f version="$version" -f macos=false

run_id=""
for _ in $(seq 1 30); do
    run_id=$(gh run list --workflow build --event workflow_dispatch --limit 5 \
                 --json databaseId,createdAt,headSha \
                 --jq "[.[] | select(.createdAt >= \"$started\" and .headSha == \"$sha\")][0].databaseId // empty")
    [[ -n "$run_id" ]] && break
    sleep 2
done
[[ -n "$run_id" ]] || die "could not find the started workflow run – check the Actions tab"

echo "▸ waiting for run $run_id (Linux + Windows, usually 15–25 min)"
if gh run watch "$run_id" --exit-status --interval 30 >/dev/null; then
    echo "✓ released: $(gh release view "$tag" --json url --jq .url)"
else
    die "the GitHub build failed. The draft $tag keeps the macOS packages. A flaky failure: gh run rerun $run_id. Needs a code fix: delete the draft (gh release delete $tag), fix, push and run this again"
fi
