#!/bin/sh
find ../Source/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i

