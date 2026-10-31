#!/bin/sh
# Run the test suite with a hard per-test timeout.
#
# A deadlocked test must fail within two minutes, never leave the autopilot
# waiting on a command that will not return. Override with TEST_TIMEOUT or
# BUILD_DIR when needed.
set -e

BUILD_DIR="${BUILD_DIR:-build}"
TEST_TIMEOUT="${TEST_TIMEOUT:-120}"

exec ctest --test-dir "$BUILD_DIR" --output-on-failure --timeout "$TEST_TIMEOUT" "$@"
