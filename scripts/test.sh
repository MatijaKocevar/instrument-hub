#!/bin/sh
set -e

BUILD_DIR="${BUILD_DIR:-build-tests}"
TEST_TIMEOUT="${TEST_TIMEOUT:-120}"

exec ctest --test-dir "$BUILD_DIR" --output-on-failure --timeout "$TEST_TIMEOUT" "$@"
