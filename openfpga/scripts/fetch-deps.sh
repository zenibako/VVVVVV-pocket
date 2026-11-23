#!/usr/bin/env bash
# Fetch the pinned build inputs for the Pocket port into openfpga/.deps.
# Nothing fetched here is committed; every input is pinned by commit ID.
set -euo pipefail

OPENFPGA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="$OPENFPGA_ROOT/.deps"
DIABLO_DIR="$DEPS_DIR/diablo"
FORK_DIR="$DEPS_DIR/openfpgaCore"
SDL_DIR="$DEPS_DIR/sdl3"
# What the build uses (SDK_ROOT): Diablo's SDK sources, libraries and
# container tooling, with the fork's API headers on top.  Same pins as
# pico8pocket, so the ports share one runtime.
STAGED_SDK_DIR="$DEPS_DIR/openfpgaos-sdk"

DIABLO_URL="https://github.com/openfpgaOS/Diablo.git"
DIABLO_COMMIT="8cb3198d50618696e07f5b49ad086a369dc57706"
DIABLO_PATHS=(src/sdk tools)
FORK_URL="https://github.com/zenibako/openfpgaCore.git"
FORK_COMMIT="19d9d50ceec15771b16386456c4a3a22c355f0a5"
FORK_PATHS=(src/firmware/api)
# SDL3 headers only (zlib license): the port implements the subset of the
# API the game calls on top of openfpgaOS.  Same version as the Android CI.
SDL_URL="https://github.com/libsdl-org/SDL.git"
SDL_COMMIT="829a65d769d935c4852f8159e964312c0957260a" # release-3.4.18
SDL_PATHS=(include LICENSE.txt)
# The Pocket runtime published from the fork commit, as pico8pocket uses.
# 19d9d50 runs os20 at 100 MHz (was 90) with faster division and SDRAM
# open-row tracking, and keeps 902a8ad's save-slot fsync.
# The loader, OS and bitstream must come from the same build: the bitstream
# holds the OS's .fasttext and the bootloader as well as the FPGA logic.
RUNTIME_DIR="$DEPS_DIR/pocket-runtime"
RUNTIME_URL="https://github.com/zenibako/openfpgaCore/releases/download/pocket-runtime-19d9d50"
RUNTIME_FILES=(os20.rbf_r os.bin loader.bin)
RUNTIME_SHA256=(
    "94d3f547d741a6929835e86dc73aa2ff46e069cddfaadb103206df49de5282c4"
    "da2cd963a38ee52460688c975433f55a0a5f84759db5237a583fda6c955c96f3"
    "fc1f8d37eb0fb322006b4428f0b59d2801c9b74978bd3f24103f5f9bbfca3c99"
)

# Shallow, blobless, sparse checkout of a pinned revision.
fetch_sparse() {
    local name="$1" url="$2" commit="$3" destination="$4"
    shift 4

    if [[ -d "$destination/.git" ]]; then
        local current
        current="$(git -C "$destination" rev-parse HEAD)"
        if [[ "$current" != "$commit" ]]; then
            echo "$name is at $current, expected $commit" >&2
            echo "Move $destination aside and run make deps again." >&2
            return 1
        fi
        echo "$name: pinned revision already present"
        return 0
    fi
    if [[ -e "$destination" ]]; then
        echo "$destination exists but is not a Git checkout" >&2
        return 1
    fi

    # Stage so an interrupted fetch never leaves a partial checkout.
    local staging="$destination.partial"
    rm -rf "$staging"
    git init -q "$staging"
    git -C "$staging" remote add origin "$url"
    git -C "$staging" sparse-checkout set --no-cone "$@"
    git -C "$staging" fetch -q --depth 1 --filter=blob:none origin "$commit"
    git -C "$staging" checkout -q --detach "$commit"
    mv "$staging" "$destination"
    echo "$name: checked out $commit"
}

sha256_of() {
    if command -v sha256sum >/dev/null; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

# Download each runtime file once and check it against its pin.
fetch_runtime() {
    local i file
    mkdir -p "$RUNTIME_DIR"
    for i in "${!RUNTIME_FILES[@]}"; do
        file="${RUNTIME_FILES[$i]}"
        if [[ -f "$RUNTIME_DIR/$file" && "$(sha256_of "$RUNTIME_DIR/$file")" == "${RUNTIME_SHA256[$i]}" ]]; then
            continue
        fi
        curl -fsSL -o "$RUNTIME_DIR/$file.partial" "$RUNTIME_URL/$file"
        if [[ "$(sha256_of "$RUNTIME_DIR/$file.partial")" != "${RUNTIME_SHA256[$i]}" ]]; then
            echo "Pocket runtime $file does not match its pinned checksum" >&2
            rm -f "$RUNTIME_DIR/$file.partial"
            return 1
        fi
        mv "$RUNTIME_DIR/$file.partial" "$RUNTIME_DIR/$file"
        echo "Pocket runtime: downloaded $file"
    done
}

# Assemble the SDK the build uses.  Rebuilt from scratch whenever a pin
# changes, so it never mixes files from two pins.
stage_sdk() {
    local stamp="$STAGED_SDK_DIR/.vvvvvv-inputs"
    local inputs="$DIABLO_COMMIT $FORK_COMMIT"
    if [[ -f "$stamp" && "$(cat "$stamp")" == "$inputs" ]]; then
        echo "openfpgaOS SDK: staged SDK already up to date"
        return 0
    fi
    local staging="$STAGED_SDK_DIR.partial" header
    rm -rf "$staging"
    mkdir -p "$staging"
    cp -R "$DIABLO_DIR/src" "$DIABLO_DIR/tools" "$staging/"
    # Same filter as openfpgaOS's `make sdk`: every header, subdirectories too.
    (cd "$FORK_DIR/src/firmware/api" && find . -name '*.h' -print) |
        while read -r header; do
            mkdir -p "$staging/src/sdk/include/$(dirname "$header")"
            cp "$FORK_DIR/src/firmware/api/$header" "$staging/src/sdk/include/$header"
        done
    printf '%s\n' "$inputs" > "$staging/.vvvvvv-inputs"
    rm -rf "$STAGED_SDK_DIR"
    mv "$staging" "$STAGED_SDK_DIR"
    echo "openfpgaOS SDK: staged $STAGED_SDK_DIR"
}

mkdir -p "$DEPS_DIR"
fetch_sparse "Diablo SDK" "$DIABLO_URL" "$DIABLO_COMMIT" \
    "$DIABLO_DIR" "${DIABLO_PATHS[@]/#//}"
fetch_sparse "openfpgaOS fork API" "$FORK_URL" "$FORK_COMMIT" \
    "$FORK_DIR" "${FORK_PATHS[@]/#//}"
fetch_sparse "SDL3 headers" "$SDL_URL" "$SDL_COMMIT" \
    "$SDL_DIR" "${SDL_PATHS[@]/#//}"
stage_sdk
fetch_runtime
