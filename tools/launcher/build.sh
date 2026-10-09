#!/bin/bash
# Build "Black Ops Launcher.app" (Launcher.swift) into a game folder, next to the
# blackops that tools/make_portable.sh installs there.
#
# usage: tools/launcher/build.sh <game dir>
set -euo pipefail

game=${1:?usage: tools/launcher/build.sh <game dir>}
here=$(cd "$(dirname "$0")" && pwd)
app="$game/Black Ops Launcher.app"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

swiftc -O -parse-as-library -target arm64-apple-macos13.0 "$here/Launcher.swift" -o "$work/BlackOpsLauncher"

rm -rf "$app"
mkdir -p "$app/Contents/MacOS"
cp "$work/BlackOpsLauncher" "$app/Contents/MacOS/BlackOpsLauncher"
cat > "$app/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>BlackOpsLauncher</string>
    <key>CFBundleIdentifier</key>
    <string>local.kisakblack.launcher</string>
    <key>CFBundleName</key>
    <string>Black Ops Launcher</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
codesign --force --sign - "$app" 2>/dev/null
echo "installed $app"
