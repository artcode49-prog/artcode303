#!/usr/bin/env bash
# Build ACID-303 (VST3 + offline renderer + verification tools).
#
#   ./build.sh              configure and build everything
#   ./build.sh --install    also copy the plugin into ~/Library/Audio/Plug-Ins/VST3
#   ./build.sh --tests      build, then run the DSP self test and the VST3 host test
#
# JUCE is picked up from $JUCE_PATH, or a sibling ../JUCE checkout, otherwise it
# is downloaded automatically by CMake.
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-Build}"
CONFIG="${CONFIG:-Release}"

find_juce() {
    if [[ -n "${JUCE_PATH:-}" && -d "${JUCE_PATH}" ]]; then echo "${JUCE_PATH}"; return; fi
    for candidate in "../JUCE" "$HOME/JUCE" "/usr/local/JUCE"; do
        if [[ -f "${candidate}/CMakeLists.txt" ]]; then echo "${candidate}"; return; fi
    done
    echo ""
}

JUCE_DIR="$(find_juce)"

if [[ -n "${JUCE_DIR}" ]]; then
    echo "==> using JUCE at ${JUCE_DIR}"
    cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${CONFIG}" -DJUCE_PATH="${JUCE_DIR}"
else
    echo "==> no local JUCE found, CMake will download it"
    cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${CONFIG}"
fi

cmake --build "${BUILD_DIR}" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

VST3="${BUILD_DIR}/Acid303_artefacts/${CONFIG}/VST3/Acid303.vst3"
echo
echo "==> built:"
echo "    ${VST3}"
echo "    ${BUILD_DIR}/tb303_render        (offline renderer / self test)"
echo "    ${BUILD_DIR}/tb303_hosttest_artefacts/${CONFIG}/tb303_hosttest"

if [[ "${1:-}" == "--install" ]]; then
    DEST="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "${DEST}"
    rm -rf "${DEST}/Acid303.vst3"
    cp -R "${VST3}" "${DEST}/"
    echo "==> installed to ${DEST}"
fi

if [[ "${1:-}" == "--tests" || "${2:-}" == "--tests" ]]; then
    echo
    echo "==> DSP self test"
    "${BUILD_DIR}/tb303_render" selftest
    echo "==> VST3 host test"
    "${BUILD_DIR}/tb303_hosttest_artefacts/${CONFIG}/tb303_hosttest" "${VST3}" /tmp/acid303_check.wav 6 130
fi
