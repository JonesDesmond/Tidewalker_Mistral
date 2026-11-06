#!/bin/sh
# Installs tidewalker for the current user without root:
# binary -> ~/.local/bin, launcher + icon -> ~/.local/share
# Usage: packaging/linux/install.sh [path-to-tidewalker-binary]
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${1:-$ROOT/build/tidewalker}"
if [ ! -x "$BIN" ]; then
    echo "binary not found: $BIN (build first: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j)"
    exit 1
fi
python3 "$ROOT/tools/make_icon.py"
mkdir -p "$HOME/.local/bin" "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/64x64/apps"
cp "$BIN" "$HOME/.local/bin/tidewalker"
chmod +x "$HOME/.local/bin/tidewalker"
cp "$ROOT/packaging/linux/tidewalker-icon.png" "$HOME/.local/share/icons/hicolor/64x64/apps/tidewalker.png"
cat > "$HOME/.local/share/applications/tidewalker.desktop" <<EOF
[Desktop Entry]
Name=Tidewalker
Comment=Open-world tide adventure: the map changes on a clock
Exec=$HOME/.local/bin/tidewalker
Icon=tidewalker
Terminal=false
Type=Application
Categories=Game;AdventureGame;
EOF
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi
echo "installed: $HOME/.local/bin/tidewalker"
echo "run with: tidewalker   (make sure ~/.local/bin is in PATH)"
