#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# ALCUBIERRE VST3 / AU Installer
# Finis Musicae Corp
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Prefer pre-built dist/ if present, otherwise fall back to build artefacts
if [ -d "$SCRIPT_DIR/dist/ALCUBIERRE.vst3" ]; then
    PLUGIN_DIR="$SCRIPT_DIR/dist"
elif [ -d "$SCRIPT_DIR/build/ALCUBIERRE_artefacts/Release/VST3/ALCUBIERRE.vst3" ]; then
    PLUGIN_DIR="$SCRIPT_DIR/build/ALCUBIERRE_artefacts/Release"
    VST3_SRC="$PLUGIN_DIR/VST3/ALCUBIERRE.vst3"
    AU_SRC="$PLUGIN_DIR/AU/ALCUBIERRE.component"
else
    echo "❌  No built plugins found."
    echo "    Either clone with dist/ folder, or build first:"
    echo "    cmake -B build -G 'Unix Makefiles' && cmake --build build --config Release"
    exit 1
fi

# Normalise paths (dist/ has them flat)
VST3_SRC="${VST3_SRC:-$PLUGIN_DIR/ALCUBIERRE.vst3}"
AU_SRC="${AU_SRC:-$PLUGIN_DIR/ALCUBIERRE.component}"

VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"

echo ""
echo "  ╔═══════════════════════════════════════╗"
echo "  ║   ALCUBIERRE  —  Finis Musicae Corp   ║"
echo "  ║         Spacetime Warp Plugin          ║"
echo "  ╚═══════════════════════════════════════╝"
echo ""

mkdir -p "$VST3_DEST" "$AU_DEST"

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
    killall -9 AudioComponentRegistrar 2>/dev/null || true
fi

# ─── Clear Gatekeeper quarantine (required on macOS — no code signing) ───────
echo "  → Clearing Gatekeeper quarantine ..."
xattr -dr com.apple.quarantine "$VST3_DEST/ALCUBIERRE.vst3"   2>/dev/null || true
xattr -dr com.apple.quarantine "$AU_DEST/ALCUBIERRE.component" 2>/dev/null || true
echo "  ✓ Quarantine cleared"

echo ""
echo "  ✅  ALCUBIERRE installed successfully!"
echo ""
echo "  In Ableton Live:"
echo "    Restart Ableton (or Preferences → Plug-Ins → rescan VST3)"
echo "    Find ALCUBIERRE in the plugin browser under effects"
echo ""
echo "  In Logic Pro / GarageBand (AU):"
echo "    Plugins are in ~/Library/Audio/Plug-Ins/Components"
echo ""
