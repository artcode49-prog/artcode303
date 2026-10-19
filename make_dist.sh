#!/usr/bin/env bash
# Builds the distributable set in Dist/.
#
#   ./make_dist.sh                 macOS from Build/, Windows from the latest CI run
#   ./make_dist.sh path/to/win.vst3  use a Windows bundle you already have
#
# The set contains both VST3 builds, the documentation, the demo WAVs and a
# source snapshot (AGPLv3 requires the Corresponding Source to travel with the
# binary), zipped as Dist/Acid303-<version>-macOS-Windows.zip.
#
# The macOS build is signed with a "Developer ID Application" identity when one
# is installed. Set ACID303_NOTARY_PROFILE to a notarytool keychain profile to
# also notarize it, which is what lets other people use the download without
# Gatekeeper complaining ("unidentified developer" / "damaged").
set -euo pipefail

cd "$(cd "$(dirname "$0")" && pwd)"

repo_slug="artcode49-prog/artcode303"
version="$(sed -n 's/^project([^ ]* VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
[ -n "$version" ] || { echo "could not read the version from CMakeLists.txt"; exit 1; }

name="Acid303-$version"
set_dir="Dist/$name"
mac_plugin="Build/Acid303_artefacts/Release/VST3/Acid303.vst3"

[ -d "$mac_plugin" ] || { echo "macOS build not found: $mac_plugin"; echo "build it first: cmake -S . -B Build -G Xcode && cmake --build Build --config Release"; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

#---------------------------------------------------------------- Windows build
if [ $# -ge 1 ]; then
    win_plugin="$1"
else
    echo "downloading the Windows build from the latest CI run..."
    gh run download -R "$repo_slug" -n Acid303-Windows-VST3 -D "$work/dl"
    zip_file="$(find "$work/dl" -name '*.zip' | head -1)"
    [ -n "$zip_file" ] || { echo "no Acid303-Windows-VST3 artifact found"; exit 1; }
    unzip -q "$zip_file" -d "$work/win"
    win_plugin="$work/win/Acid303.vst3"
fi

[ -d "$win_plugin" ] || { echo "Windows bundle not found: $win_plugin"; exit 1; }

#---------------------------------------------------------------- assemble
rm -rf "$set_dir"
mkdir -p "$set_dir/macOS" "$set_dir/Windows" "$set_dir/demo" "$set_dir/source"

cp -R "$mac_plugin" "$set_dir/macOS/"
cp -R "$win_plugin" "$set_dir/Windows/"
cp README.md LICENSE INSTALL-ja.txt INSTALL-en.txt "$set_dir/"
cp Presets/*.wav "$set_dir/demo/"

# Source snapshot for AGPL compliance (demo wavs already live in demo/).
git archive --format=tar.gz --prefix="$name/" -o "$set_dir/source/$name-source.tar.gz" HEAD ':(exclude)Presets'

url="$(git remote get-url origin 2>/dev/null || echo "https://github.com/$repo_slug")"
url="${url#git@github.com:}"; url="${url%.git}"

cat > "$set_dir/SOURCE.txt" <<EOF
ACID-303 (VST3) - Corresponding Source
======================================

Licensed under the GNU Affero General Public License v3.0 or later.
See LICENSE for the full text.

Corresponding source: $url

SPDX-License-Identifier: AGPL-3.0-or-later
EOF

# No mac metadata inside the distributed set.
find "$set_dir" -name '.DS_Store' -delete
xattr -cr "$set_dir" 2>/dev/null || true

#---------------------------------------------------------------- macOS signing
# A Developer ID signature plus a stapled notarization ticket is what stops
# Gatekeeper from calling the download "damaged" or "from an unidentified
# developer" on somebody else's Mac.
mac_bundle="$set_dir/macOS/Acid303.vst3"

sign_identity="${ACID303_SIGN_IDENTITY:-}"
if [ -z "$sign_identity" ]; then
    sign_identity="$(security find-identity -v -p codesigning 2>/dev/null \
        | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -1)"
fi

if [ -n "$sign_identity" ]; then
    echo "signing the macOS build with: $sign_identity"
    codesign --force --options runtime --timestamp --sign "$sign_identity" "$mac_bundle"
    codesign --verify --strict "$mac_bundle"
else
    echo "WARNING: no 'Developer ID Application' identity found, so the macOS build"
    echo "         stays ad-hoc signed. Gatekeeper will reject it on other Macs."
fi

if [ -n "${ACID303_NOTARY_PROFILE:-}" ] && [ -n "$sign_identity" ]; then
    echo "notarizing with keychain profile '$ACID303_NOTARY_PROFILE' (takes a few minutes)..."
    ditto -c -k --norsrc --noextattr --keepParent "$mac_bundle" "$work/notarize.zip"
    xcrun notarytool submit "$work/notarize.zip" --keychain-profile "$ACID303_NOTARY_PROFILE" --wait
    xcrun stapler staple "$mac_bundle"
    xcrun stapler validate "$mac_bundle"
    echo "notarization ticket stapled to the plugin"
elif [ -n "$sign_identity" ]; then
    echo "NOTE: the build is signed but NOT notarized yet, so Gatekeeper will still"
    echo "      block it on other Macs. Store your credentials once:"
    echo "        xcrun notarytool store-credentials ACID303 --apple-id <your Apple ID> --team-id BDKDD2PV79"
    echo "      then re-run:  ACID303_NOTARY_PROFILE=ACID303 ./make_dist.sh"
fi

#---------------------------------------------------------------- macOS disk image
# A .zip full of Mach-O code is what browser/AV heuristics tend to look at
# hardest. A signed, notarized .dmg is the usual way to hand a macOS build to
# someone (and it installs with a drag-and-drop).
mac_dmg="Dist/$name-macOS.dmg"

if command -v hdiutil >/dev/null 2>&1; then
    dmg_stage="$work/dmg"
    rm -rf "$dmg_stage"
    mkdir -p "$dmg_stage"
    cp -R "$mac_bundle" "$dmg_stage/Acid303.vst3"
    cp "$set_dir/INSTALL-ja.txt" "$set_dir/INSTALL-en.txt" \
       "$set_dir/LICENSE" "$set_dir/SOURCE.txt" "$dmg_stage/"
    find "$dmg_stage" -name '.DS_Store' -delete
    rm -f "$mac_dmg"
    hdiutil create -quiet -volname "ACID-303 $version" \
        -srcfolder "$dmg_stage" -ov -format UDZO "$mac_dmg"

    if [ -n "$sign_identity" ]; then
        codesign --force --timestamp --sign "$sign_identity" "$mac_dmg"
    fi

    if [ -n "${ACID303_NOTARY_PROFILE:-}" ]; then
        echo "notarizing the disk image..."
        xcrun notarytool submit "$mac_dmg" --keychain-profile "$ACID303_NOTARY_PROFILE" --wait
        xcrun stapler staple "$mac_dmg"
        xcrun stapler validate "$mac_dmg"
    fi
fi

#---------------------------------------------------------------- zip
# One archive per platform, plus the combined set. Keeping them apart makes it
# obvious which build an operating system is unhappy about: the macOS bundle is
# signed and notarized, the Windows DLL is not (no code signing certificate).
zip_set() {
    local src="$1" dest="$2"
    rm -f "$dest"
    ditto -c -k --norsrc --noextattr --keepParent "$src" "$dest"
}

# A standalone, AGPL-complete package for one platform.
stage_platform() {
    local platform="$1" dest="$2"
    local stage="$work/pkg-$platform"
    rm -rf "$stage"
    mkdir -p "$stage/$name-$platform"
    cp -R "$set_dir/$platform/Acid303.vst3" "$stage/$name-$platform/"
    cp "$set_dir/README.md" "$set_dir/LICENSE" "$set_dir/SOURCE.txt" \
       "$set_dir/INSTALL-ja.txt" "$set_dir/INSTALL-en.txt" "$stage/$name-$platform/"
    cp -R "$set_dir/demo" "$set_dir/source" "$stage/$name-$platform/"
    find "$stage" -name '.DS_Store' -delete
    zip_set "$stage/$name-$platform" "$dest"
}

all_zip="Dist/$name-macOS-Windows.zip"
mac_zip="Dist/$name-macOS.zip"
win_zip="Dist/$name-Windows.zip"

zip_set "$set_dir" "$all_zip"
stage_platform "macOS" "$mac_zip"
stage_platform "Windows" "$win_zip"

echo
echo "set:         $set_dir"
echo "macOS dmg:   $mac_dmg ($(du -h "$mac_dmg" | cut -f1))"
echo "macOS:       $mac_zip ($(du -h "$mac_zip" | cut -f1))"
echo "Windows:     $win_zip ($(du -h "$win_zip" | cut -f1))"
echo "both OSes:   $all_zip ($(du -h "$all_zip" | cut -f1))"
