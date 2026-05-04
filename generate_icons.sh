#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SRC_ICON=${ROOT_DIR}/logo.png
BUILD_DIR=${ROOT_DIR}/build-rmp

mkdir -p "$BUILD_DIR/bin"

if [[ "$OSTYPE" == darwin* ]]; then
  if command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1; then
    ICONSET_DIR="$BUILD_DIR/Icon.iconset"
    rm -rf "$ICONSET_DIR"
    mkdir -p "$ICONSET_DIR"

    # Crea versione ritagliata del logo (rimuove spazio trasparente attorno)
    CROPPED_ICON="$BUILD_DIR/logo_cropped.png"
    if command -v magick >/dev/null 2>&1 || command -v convert >/dev/null 2>&1; then
      MAGICK=$(command -v magick || command -v convert)
      "$MAGICK" "$SRC_ICON" -trim -background none "$CROPPED_ICON"
      echo "Cropped logo using ImageMagick"
    else
      # Fallback: usa il logo originale (installa ImageMagick per ritaglio automatico)
      cp "$SRC_ICON" "$CROPPED_ICON"
      echo "WARNING: ImageMagick non trovato. Installalo per ritaglio automatico: brew install imagemagick"
    fi

    sips -z 16 16     "$CROPPED_ICON" --out "$ICONSET_DIR/icon_16x16.png"
    sips -z 32 32     "$CROPPED_ICON" --out "$ICONSET_DIR/icon_16x16@2x.png"
    sips -z 32 32     "$CROPPED_ICON" --out "$ICONSET_DIR/icon_32x32.png"
    sips -z 64 64     "$CROPPED_ICON" --out "$ICONSET_DIR/icon_32x32@2x.png"
    sips -z 128 128   "$CROPPED_ICON" --out "$ICONSET_DIR/icon_128x128.png"
    sips -z 256 256   "$CROPPED_ICON" --out "$ICONSET_DIR/icon_128x128@2x.png"
    sips -z 256 256   "$CROPPED_ICON" --out "$ICONSET_DIR/icon_256x256.png"
    sips -z 512 512   "$CROPPED_ICON" --out "$ICONSET_DIR/icon_256x256@2x.png"
    sips -z 512 512   "$CROPPED_ICON" --out "$ICONSET_DIR/icon_512x512.png"
    sips -z 1024 1024 "$CROPPED_ICON" --out "$ICONSET_DIR/icon_512x512@2x.png"
    iconutil -c icns "$ICONSET_DIR" -o "$BUILD_DIR/bin/logo.icns"
    cp "$BUILD_DIR/bin/logo.icns" "$ROOT_DIR/logo.icns"
    rm -rf "$ICONSET_DIR" "$CROPPED_ICON"
    echo "Generated $BUILD_DIR/bin/logo.icns and $ROOT_DIR/logo.icns"
  else
    echo "sips/iconutil not found; cannot generate .icns on macOS"
    exit 1
  fi
else
  if command -v magick >/dev/null 2>&1 || command -v convert >/dev/null 2>&1; then
    MAGICK=$(command -v magick || command -v convert)
    "$MAGICK" "$SRC_ICON" -define icon:auto-resize=256,128,64,48,32,16 "$BUILD_DIR/bin/logo.ico"
    cp "$BUILD_DIR/bin/logo.ico" "$ROOT_DIR/logo.ico"
    echo "Generated $BUILD_DIR/bin/logo.ico and $ROOT_DIR/logo.ico"
  else
    echo "ImageMagick not found; cannot generate .ico"
    exit 1
  fi
fi
