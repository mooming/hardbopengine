#!/usr/bin/env bash
# Build, run, and if the binary stalls, sample ITS stacks (not a shell's) and say where it is stuck.
set -u
CFG="${1:?usage: hang.sh <Config> <patienceSeconds> [runner args...]}"
shift
WAIT="${1:?patience seconds}"
shift
BUILD_LOG='build/gate/hang-build.log'

cmake --build build --config "$CFG" --target EngineTest > "$BUILD_LOG" 2>&1
if grep -q 'error:' "$BUILD_LOG"; then
	echo 'REFUSED: build failed - not running a stale binary'
	grep -m3 'error:' "$BUILD_LOG" | cut -c1-140
	exit 90
fi

BIN='build/Applications/EngineTest/'$CFG'/EngineTest'
OUT='build/gate/hang-run.log'

"$BIN" "$@" > "$OUT" 2>&1 &
PID=$!

ELAPSED=0
while [ "$ELAPSED" -lt "$WAIT" ]; do
	if ! kill -0 "$PID" 2>/dev/null; then
		break
	fi
	sleep 1
	ELAPSED=$((ELAPSED + 1))
done

if kill -0 "$PID" 2>/dev/null; then
	echo "STALLED after ${WAIT}s at pid ${PID} - sampling that pid"
	sample "$PID" 3 -mayDie > 'build/gate/hang-sample.txt' 2>&1
	grep -a 'hbe::' 'build/gate/hang-sample.txt' | head -25 | cut -c1-120
	echo '--- last log lines ---'
	tail -6 "$OUT" | cut -c1-120
	kill -9 "$PID" 2>/dev/null
	exit 70
fi


# A cmake --build here can re-configure without -DTEST_ENABLED, which silently removes every test body from the
# executable. A verdict from such a binary is worthless, so say so instead of printing "FINISHED".
if grep -aq "test bodies live in the library sources" "$OUT"; then
	echo "REFUSED: the binary printed the missing -test advice, so it contains no test body. Build with:"
	echo "         ./build.sh Applications/EngineTest -test -debug -dev -release"
	exit 99
fi

wait "$PID"
CODE=$?
SUMMARY=$(grep -a 'collections passed' "$OUT" | tail -1)
echo "FINISHED exit=$CODE : $SUMMARY"
