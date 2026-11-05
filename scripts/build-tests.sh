#!/bin/sh
set -e

BUILD_DIR="${BUILD_DIR:-build-tests}"
JOBS="${IH_BUILD_JOBS:-$(nproc)}"

RUNNERS="ihub-runner-audio ihub-runner-dsp ihub-runner-sampler ihub-runner-app ihub-runner-project ihub-runner-api ihub-runner-session ihub-runner-gui ihub-runner-rt"

ISOLATED="ihub-audio-engine-test ihub-instrument-test ihub-fx-chain-test ihub-slot-switcher-test ihub-warm-pool-test ihub-session-concurrent-instances-test ihub-session-sandbox-death-test ihub-e2e-test ihub-plugin-instance-test ihub-testplugin-scan-test ihub-sandbox-test ihub-launchpad-protocol-test ihub-launchpad-mapping-test ihub-selftest ihub-ui-snapshot-test ihub-ui-pages-test"

if [ "$#" -gt 0 ]; then
    TARGETS="$*"
else
    TARGETS="$RUNNERS $ISOLATED"
fi

exec cmake --build "$BUILD_DIR" --parallel "$JOBS" --target $TARGETS
