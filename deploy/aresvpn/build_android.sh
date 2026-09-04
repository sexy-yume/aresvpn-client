#!/bin/bash
# AresVPN Client - the BRANDED Android build (AresProject ROADMAP 18-3g, after its stop line).
#
# 18-3g's measurement was deliberately made with `deploy/build.sh` byte-unchanged - upstream's own
# script, the fork unchanged - and it is green: an APK builds. That build carries UPSTREAM's name,
# because `deploy/build.sh` does not pass `-C cmake/aresvpn/branding.cmake` and nothing in it can:
# a cache preload is a cmake COMMAND-LINE argument and the script's argument list is fixed.
#
# So this is the Android sibling of `deploy/aresvpn/build_windows.cmd`, which solves the same
# problem the same way: call cmake ourselves, with the preload, and pass exactly the arguments
# `deploy/build.sh -t android` computes. Those arguments are not guessed - they are the ones it
# printed on the green run (`run_traced` echoes the whole command line), which is the only source
# for them that cannot drift from what was measured.
#
# WHY NOT PATCH deploy/build.sh: #D177 rule 3. Every upstream file this fork edits is a merge
# conflict for ever, and the seam that avoids one already exists - upstream added the branding
# CLIENT_* variables themselves, and every one of them is read behind an `if(NOT CLIENT_...)`.
#
# NOTHING HERE MAY BE CONVEYED. #D178 forbids an Android binary while ML Kit is linked, and the
# signing key its release build needs is a throwaway made by the caller.
set -eu

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/deploy/build-android}"
ABI="${ABI:-arm64-v8a}"

: "${QT_ROOT_PATH:?set QT_ROOT_PATH, e.g. /opt/Qt/6.10.3}"
: "${QT_HOST_PATH:?set QT_HOST_PATH, e.g. /opt/Qt/6.10.3/gcc_64 - a cross build runs the HOST Qt tools}"
: "${ANDROID_SDK_ROOT:?set ANDROID_SDK_ROOT}"
: "${ANDROID_NDK_ROOT:?set ANDROID_NDK_ROOT}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-28}"

case "$ABI" in
    arm64-v8a)   QT_ABI_DIR=android_arm64_v8a ;;
    armeabi-v7a) QT_ABI_DIR=android_armv7 ;;
    x86_64)      QT_ABI_DIR=android_x86_64 ;;
    x86)         QT_ABI_DIR=android_x86 ;;
    *) echo "FATAL: unsupported ABI \"$ABI\""; exit 2 ;;
esac

TOOLCHAIN="${QT_ROOT_PATH}/${QT_ABI_DIR}/lib/cmake/Qt6/qt.toolchain.cmake"
[ -f "$TOOLCHAIN" ] || { echo "FATAL: no Qt android toolchain at $TOOLCHAIN"; exit 2; }
[ -x "${QT_HOST_PATH}/libexec/qmlimportscanner" ] || { echo "FATAL: QT_HOST_PATH has no qmlimportscanner - it must be a DESKTOP Qt of the same version"; exit 2; }

echo "root:      $ROOT"
echo "build:     $BUILD_DIR"
echo "abi:       $ABI  ($QT_ABI_DIR)"
echo "platform:  $ANDROID_PLATFORM"
echo "ndk:       $ANDROID_NDK_ROOT"
echo "host qt:   $QT_HOST_PATH"

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -C "${ROOT}/cmake/aresvpn/branding.cmake" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$TOOLCHAIN" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DQT_ANDROID_ABIS="$ABI"

cmake --build "$BUILD_DIR" --config Release --parallel "$(nproc 2>/dev/null || echo 4)"

# THE ARTEFACT IS ASSERTED BY NAME, because the name is the whole point of this script (#L020: a
# check that cannot fail certifies nothing). An unbranded APK here means the preload did not take.
echo
echo "=== the artefact ==="
found=0
while IFS= read -r apk; do
    printf '  %s  %s bytes\n' "$apk" "$(stat -c %s "$apk")"
    case "$(basename "$apk")" in
        *AresVPNClient*) found=1 ;;
    esac
done < <(find "$BUILD_DIR" -name "*.apk" 2>/dev/null)

if [ "$found" -ne 1 ]; then
    echo "FATAL: no APK carries the AresVPNClient name - the branding preload did not take, and an"
    echo "       APK with upstream's name is what deploy/build.sh already produces. Not a pass."
    exit 4
fi
echo "BRANDED-ANDROID-BUILD-DONE"
