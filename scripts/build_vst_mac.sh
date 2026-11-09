#!/bin/bash
# Builds HH VS Musician (Reverb).vst (VST 2.4, 64-bit Intel) for Mac OS X 10.8 and later,
# then verifies the binary really declares 10.8 as its minimum and only links
# libraries that exist on 10.8. Run on macOS (GitHub's Intel runner or any Mac
# with Xcode command line tools):
#
#   scripts/build_vst_mac.sh   -> "build/HH VS Musician (Reverb).vst", build/HH-VS-Musician-Reverb-mac-vst.zip

set -euo pipefail

cd "$(dirname "$0")/.."
OUT=build
NAME="HH VS Musician (Reverb)"
EXE=HHVSMusicianReverb
ZIP=HH-VS-Musician-Reverb-mac-vst.zip
BUNDLE="$OUT/$NAME.vst"
MIN_OS=10.8

rm -rf "$OUT"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"
cp Resources/Info.plist "$BUNDLE/Contents/Info.plist"
printf 'BNDL????' > "$BUNDLE/Contents/PkgInfo"

echo "== Toolchain"
xcrun clang++ --version | head -1
echo "SDK $(xcrun --show-sdk-version) at $(xcrun --show-sdk-path)"

echo "== Compiling (x86_64, minimum Mac OS X $MIN_OS)"
xcrun clang++ -arch x86_64 -mmacosx-version-min=$MIN_OS \
    -std=c++98 -stdlib=libc++ -O2 -fvisibility=hidden -Wall -Wextra \
    -fno-objc-arc -Werror=unguarded-availability -Werror=unguarded-availability-new \
    -bundle -framework Cocoa \
    -Wl,-exported_symbol,_VSTPluginMain -Wl,-exported_symbol,_main_macho \
    Source/VST2/VSMusicianVST.cpp Source/VST2/Presets.cpp Source/VST2/PanelView.mm \
    Source/DSP/Netlist.cpp Source/DSP/SpringTank.cpp Source/DSP/MusicianAmp.cpp \
    -o "$BUNDLE/Contents/MacOS/$EXE"

BIN="$BUNDLE/Contents/MacOS/$EXE"

echo "== Verifying"
lipo -info "$BIN"
file "$BIN"

# Minimum OS as recorded by the linker (older load command or newer one)
LOADCMDS=$(otool -l "$BIN")
MINOS=$(echo "$LOADCMDS" | awk '/LC_VERSION_MIN_MACOSX/{f=1} f&&/version/{print $2; exit}')
if [ -z "$MINOS" ]; then
    MINOS=$(echo "$LOADCMDS" | awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}')
fi
echo "Declared minimum OS: ${MINOS:-unknown}"
if [ "$MINOS" != "$MIN_OS" ]; then
    echo "::error::Binary declares minimum OS '${MINOS:-unknown}', expected $MIN_OS"
    exit 1
fi
echo "::notice::$NAME.vst declares minimum Mac OS X $MINOS"

# Only system libraries and frameworks that ship with 10.8 are allowed
# (the panel uses Cocoa; -Werror=unguarded-availability above rejects any API newer than 10.8)
echo "Linked libraries:"
otool -L "$BIN" | tail -n +2
BAD=$(otool -L "$BIN" | tail -n +2 | awk '{print $1}' \
      | grep -v -E '^/usr/lib/(libc\+\+\.1|libSystem\.B|libobjc\.A)\.dylib$' \
      | grep -v -E '^/System/Library/Frameworks/(Cocoa|AppKit|Foundation|CoreFoundation|CoreGraphics|ApplicationServices|CoreServices|QuartzCore|CoreText)\.framework/' \
      || true)
if [ -n "$BAD" ]; then
    echo "::error::Unexpected library dependency: $BAD"
    exit 1
fi

# Exported entry points
nm -gU "$BIN" | grep -E '_VSTPluginMain|_main_macho'

# Every imported symbol, for the record
echo "Imported symbols:"
nm -u "$BIN"

echo "== Packaging"
(cd "$OUT" && ditto -c -k --keepParent "$NAME.vst" "$ZIP")
ls -la "$OUT"
echo "Built $BUNDLE"
