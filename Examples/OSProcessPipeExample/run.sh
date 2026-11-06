#!/bin/sh
# run.sh - OSProcess pipe example: build and run both scenarios.
# Test A: normal-exit child (expect exit 0)
# Test B: hung child (expect 2s deadline SIGKILL, exit 3)

set -u
cd "$(dirname "$0")"

CC=${CC:-cc}

echo "=== Build"
"$CC" -std=c11 -Wall -o posix_pipe_spawn posix_pipe_spawn.c || exit 1

echo "=== Test A: normal-exit child"
./posix_pipe_spawn /usr/bin/printf "hello from child\n"
statusA=$?

echo "=== Test B: hung child (2s deadline kill)"
./posix_pipe_spawn sleep 10
statusB=$?

if [ "$statusA" -eq 0 ] && [ "$statusB" -eq 3 ]; then
	echo "PASS: A=$statusA (expect 0), B=$statusB (expect 3)"
	exit 0
fi

echo "FAIL: A=$statusA (expect 0), B=$statusB (expect 3)"
exit 1
