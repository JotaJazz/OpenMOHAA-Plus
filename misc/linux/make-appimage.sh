#!/bin/sh
# Build an AppImage from a folder holding the installed game files (openmohaa, omohaaded, *.so and
# the bundled libSDL2/libopenal/libcurl), as produced by the Linux CI build.
#   misc/linux/make-appimage.sh <package dir> <x86_64|aarch64> <output .AppImage> [appimagetool]
# Without an appimagetool argument it only prints the AppDir it built (for testing AppRun).
set -eu
PACKAGE=$1
ARCH=$2
OUT=$3
TOOL=${4:-}
SRC=$(cd "$(dirname "$0")/../.." && pwd)
APPDIR=${APPDIR:-$(mktemp -d)}

mkdir -p "$APPDIR/usr/lib/openmohaa"
cp -a "$PACKAGE"/. "$APPDIR/usr/lib/openmohaa/"
cp "$SRC/misc/linux/AppRun" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"
cp "$SRC/misc/openmohaa.png" "$APPDIR/openmohaa-plus.png"
cat > "$APPDIR/openmohaa-plus.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=OpenMOHAA Plus
Comment=Open-source engine for Medal of Honor: Allied Assault (bring your own game data)
Categories=Game;Shooter;
Icon=openmohaa-plus
Exec=AppRun
Terminal=false
DESKTOP

# mktemp -d creates a 0700 folder; the AppImage must be readable by every user
chmod -R a+rX "$APPDIR"
chmod a+rx "$APPDIR/AppRun"

if [ -z "$TOOL" ]; then
    echo "$APPDIR"
    exit 0
fi
ARCH=$ARCH "$TOOL" --no-appstream "$APPDIR" "$OUT"
rm -rf "$APPDIR"
