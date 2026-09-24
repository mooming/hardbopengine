#!/bin/bash
# Run the unit-test binary under a wall clock, then say what happened - including when it says nothing.
# Usage: runtest.sh <Config> [limitSeconds]
CONFIG="${1:?usage: runtest.sh <Config> [limitSeconds]}"
LIMIT="${2:-300}"
BIN="./build/Applications/EngineTest/$CONFIG/EngineTest"
LOG="build/gate/$CONFIG-run.log"

# Build first, then judge. Asking for the verdict of a binary we did not just build is how a failed compile reports
# the previous binary's green, and a mtime heuristic cannot tell my uncommitted edit from another agent's file.
# Building here makes staleness impossible rather than detectable.
case "$CONFIG" in
	Debug) BUILD_FLAG=-debug ;;
	Dev) BUILD_FLAG=-dev ;;
	Release) BUILD_FLAG=-release ;;
	*) echo "runner REFUSED: unknown configuration $CONFIG" >&2; exit 90 ;;
esac

BUILD_LOG="build/gate/$CONFIG-build.log"
if ! ./build.sh Applications/EngineTest -test "$BUILD_FLAG" > "$BUILD_LOG" 2>&1; then
	echo "runner REFUSED: the build failed, so no verdict exists. First error:" >&2
	grep -m1 "error:" "$BUILD_LOG" >&2
	exit 90
fi



/usr/bin/perl -e '
my ($limit, @cmd) = @ARGV;
my $pid = fork();
die "fork failed: $!" unless defined $pid;
if ($pid == 0) { open(STDOUT, ">&STDOUT"); exec @cmd or exit 127; }
local $SIG{ALRM} = sub { kill "TERM", $pid; sleep 2; kill "KILL", $pid; die "wall clock limit ${limit}s exceeded\n"; };
alarm $limit;
waitpid($pid, 0);
alarm 0;
my $status = $?;
if (my $sig = ($status & 127)) { print STDERR "EngineTest died by signal $sig\n"; exit 128 + $sig; }
exit ($status >> 8);
' "$LIMIT" "$BIN" 2>&1 | tee "$LOG"
code=${PIPESTATUS[0]}

summary=$(grep -a 'collections passed' "$LOG" | tail -1)
count=$(printf '%s' "$summary" | sed -n 's/.*all \([0-9][0-9]*\) collections.*/\1/p')
passLines=$(grep -ac 'Result \[PASS\]' "$LOG")

# An exit code of 0 only means the process stopped cleanly. A run that executed no test collection, or
# printed no passing result, proves nothing however cleanly it stopped, and "all 0 collections passed" is
# grammatically a pass - so it has to be refused here rather than trusted downstream.
if [ "$code" -eq 0 ] && { [ -z "$count" ] || [ "$count" -eq 0 ] || [ "$passLines" -eq 0 ]; }; then
	echo "runner REFUSED: exit=0 but nothing was proven (summary=\"$summary\" passingResults=$passLines)" >&2
	code=98
fi

echo "runner exit=$code  $summary"

# A binary configured without -test compiles no test body at all, prints advice about exactly that, and exits 1.
# Any verdict taken from it - including "it exited quickly" - is vacuous. This cost several false green reports.
if grep -aq "test bodies live in the library sources" "$LOG"; then
	echo "REFUSED: this binary was not configured with -test, so no test body is compiled into it."
	echo "         Re-run: ./build.sh Applications/EngineTest -test -debug -dev -release"
	exit 99
fi

if [ "$code" -ne 0 ] && ! grep -aq FAILED "$LOG"; then
	echo "WARNING: exit $code with no FAILED line - the suite died without saying why. Last lines of $LOG:"
	tail -5 "$LOG" | cut -c1-108
fi

exit "$code"
