#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# ALCUBIERRE VST3 / AU Installer
# Finis Musicae Corp
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build/ALCUBIERRE_artefacts/Release"

VST3_SRC="$BUILD_DIR/VST3/ALCUBIERRE.vst3"
AU_SRC="$BUILD_DIR/AU/ALCUBIERRE.component"

VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"

# ─── Check sources exist ──────────────────────────────────────────────────────
if [ ! -d "$VST3_SRC" ] && [ ! -d "$AU_SRC" ]; then
    echo "❌  No built plugins found in $BUILD_DIR"
    echo "    Run: cmake -B build -G 'Unix Makefiles' && cmake --build build --config Release"
    exit 1
fi

echo ""
echo "  ╔═══════════════════════════════════════╗"
echo "  ║   ALCUBIERRE  —  Finis Musicae Corp   ║"
echo "  ║         Spacetime Warp Plugin          ║"
echo "  ╚═══════════════════════════════════════╝"
echo ""

# ─── Create destination directories ──────────────────────────────────────────
mkdir -p "$VST3_DEST"
mkdir -p "$AU_DEST"

# ─── Install VST3 ────────────────────────────────────────────────────────────
if [ -d "$VST3_SRC" ]; then
    echo "  → Installing VST3 to $VST3_DEST ..."
    rm -rf "$VST3_DEST/ALCUBIERRE.vst3"
    cp -R "$VST3_SRC" "$VST3_DEST/"
    echo "  ✓ ALCUBIERRE.vst3 installed"
fi

# ─── Install AU ──────────────────────────────────────────────────────────────
if [ -d "$AU_SRC" ]; then
    echo "  → Installing AU to $AU_DEST ..."
    rm -rf "$AU_DEST/ALCUBIERRE.component"
    cp -R "$AU_SRC" "$AU_DEST/"
    echo "  ✓ ALCUBIERRE.component installed"

    # Register with AudioComponentRegistrar so AU is immediately visible
    if command -v auval &>/dev/null; then
        echo "  → Registering AU with system cache ..."
        killall -9 AudioComponentRegistrar 2>/dev/null || true
        sleep 1
        echo "  ✓ AU cache refreshed"
    fi
fi

# ─── Remove macOS quarantine flag (critical for Gatekeeper) ──────────────────
echo "  → Clearing quarantine flags ..."
xattr -dr com.apple.quarantine "$VST3_DEST/ALCUBIERRE.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine "$AU_DEST/ALCUBIERRE.component" 2>/dev/null || true
echo "  ✓ Quarantine cleared"

echo ""
echo "  ✅  ALCUBIERRE installed successfully!"
echo ""
echo "  In Ableton Live:"
echo "    Preferences → Plug-Ins → VST3 Folder → scan"
echo "    (or restart Ableton — it auto-scans ~/Library/Audio/Plug-Ins/VST3)"
echo ""
echo "  As AU (Logic / Garage Band):"
echo "    Audio Units live in ~/Library/Audio/Plug-Ins/Components"
echo ""
