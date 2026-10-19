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

#---------------------------------------------------------------- zip
zip_file="Dist/$name-macOS-Windows.zip"
rm -f "$zip_file"
ditto -c -k --norsrc --noextattr --keepParent "$set_dir" "$zip_file"

echo
echo "set:     $set_dir"
echo "archive: $zip_file ($(du -h "$zip_file" | cut -f1))"
