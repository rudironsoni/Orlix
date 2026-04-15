#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Generating compile_commands.json for IXLandSystem ==="

rm -rf .clangd-cache/cdb
mkdir -p .clangd-cache/cdb

xcodegen generate --project .

xcodebuild \
    -project IXLandSystem.xcodeproj \
    -scheme IXLandSystem-6.12-arm64 \
    -sdk iphonesimulator \
    -arch arm64 \
    -configuration Debug \
    OTHER_CFLAGS="$(inherited) -gen-cdb-fragment-path $(pwd)/.clangd-cache/cdb" \
    build

echo "=== Combining fragments into compile_commands.json ==="

cd .clangd-cache/cdb

first=true
echo "[" > ../compile_commands.json

for f in *.json; do
    if [ -f "$f" ]; then
        if [ "$first" = true ]; then
            first=false
        else
            echo "," >> ../compile_commands.json
        fi
        cat "$f" >> ../compile_commands.json
    fi
done

echo "]" >> ../compile_commands.json

mv ../compile_commands.json ../../compile_commands.json

echo "=== Done: compile_commands.json created at repo root ==="
echo "Entries: $(grep -c '"command"' ../../compile_commands.json || echo 0)"