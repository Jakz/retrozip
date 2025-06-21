#!/bin/bash
set -e

# Get the absolute path to the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source and build directories
SRC_DIR="$SCRIPT_DIR/../.."
BUILD_DIR="$SCRIPT_DIR/build"

# Make sure the build directory exists
mkdir -p "$BUILD_DIR"

# Configure the project
cmake -S "$SRC_DIR" -B "$BUILD_DIR" tests

# Build the project
cmake --build "$BUILD_DIR" --target tests -- -j8
