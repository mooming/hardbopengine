#!/bin/bash
# Run the unit-test binary under a wall clock, then say what happened - including when it says nothing.
# Usage: runtest.sh <Config> [limitSeconds]
CONFIG="${1:?usage: runtest.sh <Config> [limitSeconds]}"
LIMIT="${2:-300}"
BIN="./build/Applications/EngineTest/$CONFIG/EngineTest"
LOG="build/gate/$CONFIG-run.log"

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

echo "runner exit=$code  $(grep -a 'collections passed' "$LOG" | tail -1)"

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
