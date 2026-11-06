#!/bin/sh
# Removes a user-local tidewalker install (see install.sh).
set -e
rm -f "$HOME/.local/bin/tidewalker"
rm -f "$HOME/.local/share/applications/tidewalker.desktop"
rm -f "$HOME/.local/share/icons/hicolor/64x64/apps/tidewalker.png"
echo "tidewalker (user install) removed"
echo "to remove a system .deb install instead: sudo dpkg -r tidewalker"
