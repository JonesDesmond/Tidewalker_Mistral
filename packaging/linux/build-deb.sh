#!/bin/sh
# Builds tidewalker and packages a Debian/Ubuntu .deb installer.
# Usage: packaging/linux/build-deb.sh [build-dir] [output.deb]
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build}"
OUT="${2:-$ROOT/tidewalker_1.0.0_amd64.deb}"
VERSION="1.0.0"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j "$(nproc)"
python3 "$ROOT/tools/make_icon.py"

STAGE="$ROOT/packaging/linux/stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/opt/tidewalker" "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/icons/hicolor/64x64/apps" "$STAGE/DEBIAN"
cp "$BUILD/tidewalker" "$STAGE/opt/tidewalker/tidewalker"
cp "$ROOT/packaging/linux/tidewalker.desktop" "$STAGE/usr/share/applications/tidewalker.desktop"
cp "$ROOT/packaging/linux/tidewalker-icon.png" "$STAGE/usr/share/icons/hicolor/64x64/apps/tidewalker.png"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: tidewalker
Version: $VERSION
Section: games
Priority: optional
Architecture: amd64
Maintainer: Tidewalker Dev <dev@example.com>
Depends: libc6, libstdc++6, libz1, libx11-6
Description: Tidewalker - open-world tide adventure
 On a coastal archipelago the tide goes out for miles twice a day.
 Walk the seabed at low tide, sail at high tide, and get out before
 the flood. Native C++ software-rendered 3D, no GPU required.
EOF
cat > "$STAGE/DEBIAN/postinst" <<EOF
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor || true
fi
exit 0
EOF
chmod 755 "$STAGE/DEBIAN/postinst"
dpkg-deb --root-owner-group --build "$STAGE" "$OUT"
echo "built $OUT"
dpkg-deb --info "$OUT" | head -12
