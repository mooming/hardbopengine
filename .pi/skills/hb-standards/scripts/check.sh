#!/usr/bin/env bash
# check.sh — format + strictly lint HardBop Engine sources touched by a commit,
# then prove the tree still builds in Debug, Dev and Release.
#
# Usage:
#   scripts/check.sh [--staged | <rev>] [--apply] [--no-build] [--test] [--all]
#
#   --staged     lint files staged for the next commit
#   <rev>        lint files touched by a revision (default: HEAD)
#   --apply      rewrite files with clang-format (default is check-only)
#   --no-build   skip the build gate
#   --test       also run the EngineTest suite for each configuration
#   --all        lint every source under Engine, Examples, Applications
#
# Exit status: 0 = clean, 1 = violations found, 2 = build failed, 3 = usage/environment error
#
# Design notes that are easy to get wrong, each verified on this tree:
#   * .mm / .m are EXCLUDED. clang-format classifies them as Objective-C, and the
#     repo .clang-format declares only `Language: Cpp`, so it aborts with
#     "Configuration file(s) do(es) not support Objective-C", exit 1, writing
#     nothing. A script that ignores exit status reports success while having
#     formatted none of them. Worse, run from a directory where the config is not
#     found, clang-format silently falls back to LLVM defaults and rewrites the
#     file — measured: 2249 -> 2400 bytes, wrong style, exit 0.
#   * .inl are EXCLUDED. MatrixCommonImpl.inl and VectorCommonImpl.inl are
#     #include-d *inside a class body inside a namespace*, so their one-tab
#     indentation is inherited from the includer. Formatting them standalone
#     de-indents every line to column 0.
#   * The build gate must not trust "ninja: no work to do". The touched files are
#     touched before building so a real recompile is forced and the gate cannot
#     pass vacuously.
#   * --test must build EngineTest with "build.sh ... -test". The body of main() and
#     every module's test bodies sit behind #ifdef __UNIT_TEST__, so without the flag
#     the executable has an empty main and exits zero having run nothing (measured on
#     this tree: 0 tests without -test, 285 with it). Zero tests is a failure.
#   * Neither timeout(1) nor gtimeout(1) is guaranteed to exist, and on this machine
#     neither does. Calling one unconditionally exits 127 and the gate then blames the
#     code for a missing helper binary, so probe for both and run uncapped if absent.
#   * The blank-line-after-includes rule is enforced by clang-format alone, never by
#     awk. It is position-dependent (two blanks survive before a using-directive, one
#     elsewhere) and a hand-written copy of it reported 85 findings that the formatter
#     itself disagreed with in 7 of them.
#   * Empty scope is announced, not hidden. A commit touching no C++ yields zero
#     violations without proving anything, so the verdict states that the lint
#     examined nothing rather than falling silent.

set -uo pipefail

usage() { sed -n '2,20p' "$0"; exit 3; }

REVSPEC=""
STAGED=0
APPLY=0
BUILD=1
RUNTEST=0
ALL=0

while [[ $# -gt 0 ]]; do
	case "$1" in
		--staged)   STAGED=1 ;;
		--apply)    APPLY=1 ;;
		--no-build) BUILD=0 ;;
		--test)     RUNTEST=1 ;;
		--all)      ALL=1 ;;
		-h|--help)  usage ;;
		-*)         echo "unknown option: $1" >&2; usage ;;
		*)          REVSPEC="$1" ;;
	esac
	shift
done

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || { echo "not a git tree" >&2; exit 3; }
cd "$REPO_ROOT" || exit 3
[[ -f .clang-format ]] || { echo ".clang-format not found at repo root" >&2; exit 3; }
command -v clang-format >/dev/null 2>&1 || { echo "clang-format not on PATH" >&2; exit 3; }

CLANG_FORMAT_VERSION=$(clang-format --version 2>&1 | head -1)

# A hung test binary must not hang the gate, but coreutils is not part of this
# project's toolchain. Probe for timeout(1) then gtimeout(1); if neither exists,
# run the binary uncapped. A missing helper binary is an environment gap, not a
# code violation, and must never be reported as a gate failure.
if command -v timeout >/dev/null 2>&1; then
	TIMEOUT_CMD=(timeout)
elif command -v gtimeout >/dev/null 2>&1; then
	TIMEOUT_CMD=(gtimeout)
else
	TIMEOUT_CMD=()
fi

# run_capped <seconds> <command...> — the command runs without a limit when no
# timeout helper was found above.
run_capped() {
	local secs="$1"
	shift
	if [[ ${#TIMEOUT_CMD[@]} -gt 0 ]]; then
		"${TIMEOUT_CMD[@]}" "$secs" "$@"
	else
		"$@"
	fi
}

# ------------------------------------------------------------- file selection --
INCLUDE_EXT='h|hpp|cpp|cc'
EXCLUDE_EXT='mm|m|inl'

declare -a CANDIDATES=()
if [[ $ALL -eq 1 ]]; then
	while IFS= read -r f; do CANDIDATES+=("$f"); done < <(
		find Engine Examples Applications -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cc' \) 2>/dev/null | sort)
elif [[ $STAGED -eq 1 ]]; then
	while IFS= read -r f; do [[ -n "$f" ]] && CANDIDATES+=("$f"); done < <(git diff --cached --name-only --diff-filter=ACMR)
else
	REVSPEC=${REVSPEC:-HEAD}
	git cat-file -e "${REVSPEC}^{commit}" 2>/dev/null || { echo "bad revision: $REVSPEC" >&2; exit 3; }
	while IFS= read -r f; do [[ -n "$f" ]] && CANDIDATES+=("$f"); done < <(git show --name-only --pretty=format: "$REVSPEC")
fi

declare -a FILES=()
declare -a SKIPPED=()
for f in "${CANDIDATES[@]:-}"; do
	[[ -z "$f" ]] && continue
	if [[ ! -f "$f" ]]; then continue; fi                 # deleted in this rev
	ext="${f##*.}"
	if [[ "$ext" =~ ^($EXCLUDE_EXT)$ ]]; then
		SKIPPED+=("$f ($ext — excluded: see check.sh header)")
		continue
	fi
	if head -3 "$f" 2>/dev/null | grep -qiE 'auto-?generated|do not edit'; then
		SKIPPED+=("$f (generated — header says do not edit)")
		continue
	fi
	[[ "$ext" =~ ^($INCLUDE_EXT)$ ]] && FILES+=("$f")
done

echo "=========================================================================="
echo " hb-standards — HardBop Engine"
echo " clang-format : $CLANG_FORMAT_VERSION"
echo " scope        : $(if [[ $ALL -eq 1 ]]; then echo 'Engine Examples Applications (all)'
	elif [[ $STAGED -eq 1 ]]; then echo 'staged files'
	else echo "commit ${REVSPEC:-HEAD}"; fi)"
echo " files        : ${#FILES[@]} formattable, ${#SKIPPED[@]} excluded"
echo " mode         : $([[ $APPLY -eq 1 ]] && echo 'apply' || echo 'check-only')$([[ $APPLY -eq 1 ]] || [[ $BUILD -eq 0 ]] && echo '')"
echo "=========================================================================="

for f in "${SKIPPED[@]:-}"; do [[ -n "$f" ]] && echo "  excluded : $f"; done
if [[ ${#FILES[@]} -eq 0 ]]; then
	echo "no formattable C/C++ sources in scope"
	if [[ $BUILD -eq 0 ]]; then
		# Exiting zero right here is exactly the vacuous pass this notice exists to
		# prevent, so it must be printed before leaving, not only in the verdict block.
		printf ' %s\n' 'lint scope: NONE — the lint checks above examined nothing, so this exit'
		printf ' %s\n' '                 status carries no information about style conformance.'
		exit 0
	fi
	FILES=()
fi

# ---------------------------------------------------------------- clang-format --
VIOL=0
WARN=0

# Belt and braces: clang-format enforces the Allman brace rule, the 120 column
# limit, tabs, and the blank-line conventions. Everything after this section
# covers rules clang-format structurally cannot check.
if [[ ${#FILES[@]} -gt 0 ]]; then
	if [[ $APPLY -eq 1 ]]; then
		clang-format --style=file -i "${FILES[@]}" || { echo "clang-format --apply failed" >&2; exit 1; }
		echo "[PASS] clang-format applied to ${#FILES[@]} file(s)"
	else
		cfbad=0
		cffail=""
		for f in "${FILES[@]}"; do
			d=$(clang-format --style=file "$f" 2>/dev/null | diff "$f" - 2>/dev/null | grep -c '^[<>]' || true)
			if [[ "${d:-0}" -gt 0 ]]; then
				cfbad=$((cfbad+1))
				cffail+="    $f  ($d line(s) differ — run with --apply)"$'\n'
			fi
		done
		if [[ $cfbad -eq 0 ]]; then
			echo "[PASS] clang-format — Allman braces, tabs, 120 columns, blank lines"
		else
			printf '[FAIL] clang-format — %d file(s) not conformant\n%s' "$cfbad" "$cffail"
			VIOL=$((VIOL+1))
		fi
	fi
fi

# ---------------------------------------------------- mechanical rule checks ----
# Each check is a deterministic grep/awk rule lifted verbatim from
# Engine/CodingStandards.h and docs/CodingStandards.md.
hdr() { printf '\n%-64s\n' "── $1"; }

check() { # $1 = rule name, $2 = grep -E pattern, $3 = level (FAIL|WARN), $4.. = files
	local rule="$1" pat="$2" level="$3" hits=0 matched="" f m
	shift 3
	# Engine/CodingStandards.{h,cpp} is the standard's own documentation and carries
	# deliberate BAD EXAMPLE blocks (a pessimizing std::move on return, an
	# allocation-taking ctor documented as throw-capable). Lint behaviour, not
	# documentation: formatting, naming, hygiene and includes still apply.
	local skip_behavioural=0
	case "$rule" in
		*exception*|*NRVO*) skip_behavioural=1 ;;
	esac
	for f in "$@"; do
		[[ -z "$f" ]] && continue
		if [[ $skip_behavioural -eq 1 ]]; then
			case "$f" in Engine/CodingStandards.*) continue ;; esac
		fi
		m=$(grep -nE "$pat" "$f" 2>/dev/null \
			| grep -vE '^[0-9]+:[[:space:]]*(//|\*|/\*)' \
			| grep -v 'hb-standards:ignore' || true)
		if [[ -n "$m" ]]; then
			hits=$((hits+1))
			if [[ $level == FAIL ]]; then VIOL=$((VIOL+1)); else WARN=$((WARN+1)); fi
			matched+="  $f"$'\n'"$(echo "$m" | sed 's/^/    /')"
		fi
	done
	if [[ $hits -eq 0 ]]; then
		printf '[PASS] %s\n' "$rule"
	else
		printf '[%s] %s  (%d file%s)\n%s\n' "$level" "$rule" "$hits" "$([[ $hits -eq 1 ]] && echo '' || echo 's')" "$matched"
	fi
}

if [[ ${#FILES[@]} -gt 0 ]]; then
	hdr "formatting that clang-format cannot enforce"
	check "no space-indented lines"            '^[ ]+[^ ]'                                     FAIL "${FILES[@]}"
	check "joined empty function/ctor body — braces must break" '\)[[:space:]]*((const|noexcept|override|constexpr)[[:space:]]+)*\{\}[[:space:]]*$' FAIL "${FILES[@]}"
	check "joined empty record"                '(struct|class|union|enum)\s+\w+[^;]*\{\s*\}\s*;' FAIL "${FILES[@]}"
	check "no exceptions (engine is exception-free)" '\b(throw\s+[A-Za-z_(]|try\s*\{|catch\s*\()' FAIL "${FILES[@]}"
	check "no m_ member prefix"                '\b(m_[a-z]|[a-z]+_[a-z]+\s*;)'                  WARN "${FILES[@]}"
	check "std::move on return kills NRVO"     'return\s+std::move'                              FAIL "${FILES[@]}"
	check "virtual alongside override"         'virtual\s+[^;{]*\boverride'                      FAIL "${FILES[@]}"

	hdr "naming and interface conventions"
	check "no explicit inline keyword"         '^\s*inline\s+[A-Za-z_]'                          WARN "${FILES[@]}"
	# Single-argument ctor explicitness, getter [[nodiscard]], log-before-early-return
	# and constexpr-over-magic-number are judgement calls, not greps.
	# They are listed in SKILL.md for manual review instead of being faked here.

	hdr "file hygiene"
	hyg=0
	for f in "${FILES[@]}"; do
		[[ -z "$f" ]] && continue
		head -1 "$f" | grep -q 'Copyright (c).*Hansol Park' || { echo "  $f: line 1 is not the copyright header"; hyg=$((hyg+1)); }
		[[ -z "$(tail -c 1 "$f")" ]] || { echo "  $f: does not end with a newline"; hyg=$((hyg+1)); }
		grep -qE ' +$' "$f" && { echo "  $f: trailing whitespace"; hyg=$((hyg+1)); }
	done
	[[ $hyg -eq 0 ]] && echo "[PASS] copyright header, trailing newline, no trailing whitespace" || { printf '[FAIL] file hygiene — %d issue(s)\n' "$hyg"; VIOL=$((VIOL+1)); }

	hdr "include layout — own header, then <standard>, then \"project\", each alphabetical"
	inc=0
	for f in "${FILES[@]}"; do
		[[ -z "$f" ]] && continue
		# The own header of foo.cpp is "foo.h" — matched by basename, since the repo
		# includes it both as "foo.h" and as "Dir/foo.h".
		own=$(basename "$f"); own="${own%.*}.h"
		out=$(awk -v F="$f" -v OWN="$own" '
			# phase tracks whether the top-of-file include preamble is still open. The
			# dominant repo idiom puts further #include directives inside a
			# "#ifdef __UNIT_TEST__" test block far below the first code body, so the
			# preamble must close exactly once, at the first body line, or those later
			# includes get folded into block 1 and every counter is wrong.
			BEGIN { phase = "pre" }

			# Track whether this line is a preprocessor conditional, and reset the flag on
			# a plain line so a blank after "#endif" counts normally again.
			/^#/ { guard = ($0 ~ /^[ \t]*#[ \t]*(if|ifdef|ifndef|else|elif|endif)/) ? 1 : 0 }
			!/^#/ { guard = 0 }

			# Conditionals are transparent while they hug the include region: an #ifdef that
			# directly wraps includes belongs to it, opens no block, and never ends it.
			# A blank line before the #ifdef is the tell that this is a NEW region instead -
			# in this repo that is almost always the "#ifdef __UNIT_TEST__" test block or a
			# "#ifdef PLATFORM_x" block far below the preamble. Folding those includes into
			# the preamble invents mixed <...>/"..." blocks and reports them as unsorted
			# (measured: ImportanceSampling.cpp, DefaultAllocator.cpp, WindowsDebug.cpp).
			guard == 1 { if (gap && count) phase = "body"; next }
			/^[ \t]*$/ { gap = 1; next }

			# Only "#include" opens a block. "#define" does NOT: a config header made of
			# HB_PROJECT_* macros has no includes at all, and reading its defines as
			# includes invents blocks that do not exist, which is how such a file came to
			# be reported as "block 2: not alphabetical".
			# The own header of foo.cpp is "foo.h" - matched by basename, since the repo
			# includes it both as "foo.h" and as "Dir/foo.h".
			/^[ \t]*#[ \t]*include/ {
				if (phase != "pre") next                              # a #include below the preamble belongs to a test guard
				if (count && gap) block++                             # a blank line opens a new block
				gap = 0
				if (block == 0) block = 1
				kind[block] = ($0 ~ /^[ \t]*#[ \t]*include[ \t]*</) ? "std" : "proj"
				path = $0
				sub(/^[ \t]*#[ \t]*include([ \t]*_next)?[ \t]*/, "", path)
				sub(/^["<]/, "", path)
				sub(/[">].*$/, "", path)                              # cut at the FIRST closer, not the last
				list[block] = list[block] path "\n"
				if (block == 1 && firstinc == "") firstinc = path
				count++
				next
			}

			# Any other line is the code body, and it closes the preamble for good. Until an
			# include has been seen there is no preamble to close: a copyright comment or a
			# "#pragma once" is ordinary prologue, not body.
			{ if (phase == "pre" && count) phase = "body"; next }

			END {
				nb = 0; for (b in kind) nb++
				start = 1
				# Only a genuine own header may lead. A .cpp may not use some unrelated
				# project header as an excuse to put the whole project block first.
				if (nb > 1 && kind[1] == "proj" && (firstinc == OWN || firstinc ~ ("/" OWN "$"))) start = 2

				seenProj = 0
				for (b = start; b <= nb; b++) {
					if (kind[b] == "proj") seenProj = 1
					else if (kind[b] == "std" && seenProj) { print "standard <...> block must precede the \"project\" block"; break }
				}

				for (b = 1; b <= nb; b++) {
					m = split(list[b], L, "\n"); last = ""; unsorted = 0
					for (i = 1; i <= m; i++) {
						if (L[i] == "") continue
						if (last != "" && L[i] < last) unsorted = 1
						last = L[i]
					}
					if (unsorted) print "block " b ": not alphabetical"
				}

				# The "blank lines after the include region" rule is deliberately NOT
				# reproduced here. clang-format owns it and it is position-dependent, so a
				# hand-written copy is wrong more often than right. Measured against this
				# .clang-format (MaxBlankLines unset): before a "using namespace" both 1 and
				# 2 blanks survive and 3+ collapse to 2, while before a namespace
				# declaration, a function or a comment only 1 survives. The clang-format
				# section above already reports every one of those differences, so checking
				# it twice here would only add false positives.
			}
		' "$f")
		if [[ -n "$out" ]]; then printf '[FAIL] include layout — %s\n' "$f"; echo "$out" | sed 's/^/    /'; inc=$((inc+1)); fi
	done
	[[ $inc -eq 0 ]] && echo "[PASS] include layout — own header, then <standard>, then \"project\", each alphabetical" || VIOL=$((VIOL+1))

	hdr "convention debt — reported, not gated (repository-wide, owner decision pending)"
	# A namespace body must start at column 0. Only the first non-blank line after
	# the namespace's opening brace is examined: anything deeper is class-body
	# indentation, which is legitimate and would otherwise read as a false hit.
	nsi=0
	for f in "${FILES[@]}"; do
		[[ -z "$f" ]] && continue
		bad=$(awk '
			/^namespace([[:space:]]|$)/ { wantbrace = 1; next }
			wantbrace && /^\{/    { wantbrace = 0; checknext = 1; next }
			checknext && /^[[:space:]]+[^[:space:]]/ { print; exit }
			checknext && /^[^[:space:]]/ { checknext = 0 }
		' "$f")
		[[ -n "$bad" ]] && { echo "  $f: namespace body is indented"; echo "$bad" | sed 's/^/    /'; nsi=$((nsi+1)); }
	done
	if [[ $nsi -gt 0 ]]; then
		printf '[DEBT] %d file%s indent%s namespace bodies. Rule confirmed None (owner, 2026-09-06): these are legacy files awaiting a sweep, not an open question.\n' "$nsi" "$([[ $nsi -eq 1 ]] && echo '' || echo 's')" "$([[ $nsi -eq 1 ]] && echo 's' || echo '')"
		echo "         not gated on purpose: failing every commit that touches an engine file would block"
		echo "         unrelated work. Expect --apply to de-indent such a body as part of bringing that"
		echo "         file into conformance."
	else
		echo "[PASS] namespace bodies not indented"
	fi
fi

# ------------------------------------------------------------------ build gate --
# Runs last and is mandatory unless --no-build. Proves the reformat did not break
# compilation in any of the three project configurations.
BUILD_STATUS=0
BUILD_PASS=0
BUILD_TOTAL=0
if [[ $BUILD -eq 1 ]]; then
	echo
	hdr "build gate — Debug, Dev, Release"
	TARGETS=(EngineTest VulkanExample WindowExample)

	# Force a genuine recompile so "ninja: no work to do" cannot fake a pass.
	if [[ ${#FILES[@]} -gt 0 ]]; then
		for f in "${FILES[@]}"; do [[ -n "$f" ]] && touch "$f"; done
	fi

	for t in "${TARGETS[@]}"; do
		for cfg in -dev -debug -release; do
			name=${cfg#-}; name="$(tr '[:lower:]' '[:upper:]' <<< "${name:0:1}")${name:1}"
			if out=$(./build.sh "Applications/$t" "$cfg" 2>&1); then
				BUILD_TOTAL=$((BUILD_TOTAL+1))
				vacuous=""
				grep -q "no work to do" <<<"$out" && vacuous="  (up to date — no recompile exercised)"
				BUILD_PASS=$((BUILD_PASS+1))
				printf '[PASS] %-12s %-8s%s\n' "$t" "$name" "$vacuous"
			else
				printf '[FAIL] %-12s %-8s\n' "$t" "$name"
				grep -E 'error:|FAILED' <<<"$out" | head -15 | sed 's/^/    /'
				BUILD_STATUS=1
			fi
		done
	done

	# The coding-standards exemplar is a real engine target now (see
	# Engine/CMakeLists.txt), so a drift in it fails the gate instead of drifting
	# quietly. It is not an application directory, so build.sh cannot reach it —
	# that script derives the CMake target name from the basename of the path.
	for cfg in Dev Debug Release; do
		if out=$(cmake --build build --config "$cfg" --target CodingStandards 2>&1); then
			BUILD_TOTAL=$((BUILD_TOTAL+1)); BUILD_PASS=$((BUILD_PASS+1))
			printf '[PASS] %-16s %-8s\n' "CodingStandards" "$cfg"
		else
			printf '[FAIL] %-16s %-8s\n' "CodingStandards" "$cfg"
			grep -E 'error:|FAILED' <<<"$out" | head -15 | sed 's/^/    /'
			BUILD_STATUS=1
		fi
	done

	if [[ $RUNTEST -eq 1 && $BUILD_STATUS -eq 0 ]]; then
		hdr "unit tests — EngineTest, built with -test"
		# -test is not optional here, and not for the reason it looks like. Both the body
		# of TestMain.cpp's main() and every module's test bodies sit behind
		# #ifdef __UNIT_TEST__, so a binary built without the flag has an EMPTY main: it
		# exits zero having executed nothing at all. Measured on this tree: 0 tests run
		# without -test, 285 with it. build.sh reconfigures with --fresh on every run, so
		# the defines never go sticky into a shared tree; they are configured back off at
		# the end of this block.
		if ! out=$(./build.sh Applications/EngineTest -dev -debug -release -test 2>&1); then
			printf '[FAIL] %-21s %s\n' "EngineTest" "test build (-test)"
			grep -E 'error:|FAILED' <<<"$out" | head -15 | sed 's/^/    /'
			BUILD_STATUS=1
		else
			for cfg in Dev Debug Release; do
				bin="./build/Applications/EngineTest/$cfg/EngineTest"
				log="/tmp/hb_test.$cfg"
				if [[ ! -x "$bin" ]]; then
					printf '[FAIL] %-21s %s%s\n' "EngineTest $cfg" "binary not built: " "$bin"
					BUILD_STATUS=1
					continue
				fi
				run_capped 300 "$bin" >"$log" 2>&1
				rc=$?
				# TestEnv::Report() prints the authoritative tallies. Counting lines that
				# contain PASS counted log output, not tests, so an empty suite and a suite
				# of 285 collections were both reportable as a healthy pass.
				#
				# LC_ALL=C is load-bearing, not cosmetic: the engine log is not clean UTF-8
				# (measured: an undecodable 0x80 at byte 51772 of a 229550-byte log) and BSD
				# sed aborts the whole file with "illegal byte sequence" on it. That swallowed
				# every tally and the gate then reported a suite that had in fact passed 53 of
				# 53 as never having reached Report(). Note the tallies count collections.
				total=$(LC_ALL=C sed -n 's/^# Total Count = //p' "$log" | tail -1)
				pass=$(LC_ALL=C sed -n 's/^# Pass = //p' "$log" | tail -1)
				fail=$(LC_ALL=C sed -n 's/^# Fail = //p' "$log" | tail -1)
				invalid=$(LC_ALL=C sed -n 's/^# Invalid Test = //p' "$log" | tail -1)
				if [[ $rc -eq 124 || $rc -eq 137 ]]; then
					printf '[FAIL] %-21s %s\n' "EngineTest $cfg" "timed out after 300s"
					BUILD_STATUS=1
				elif [[ -z "$pass" || -z "$total" ]]; then
					printf '[FAIL] %-21s %s\n' "EngineTest $cfg" "no report block: the suite never reached Report()"
					tail -20 "$log" | LC_ALL=C sed 's/^/    /'
					BUILD_STATUS=1
				elif [[ "$total" -eq 0 || "$pass" -eq 0 ]]; then
					printf '[FAIL] %-21s ran %s of %s collections: vacuous, so a failure\n' "EngineTest $cfg" "$pass" "$total"
					echo "         a suite that executes nothing proves nothing — built without -test?"
					BUILD_STATUS=1
				elif [[ "${fail:-0}" -ne 0 || "${invalid:-0}" -ne 0 ]]; then
					printf '[FAIL] %-21s pass=%s fail=%s invalid=%s\n' "EngineTest $cfg" "$pass" "$fail" "$invalid"
					LC_ALL=C grep -n 'FAIL' "$log" | head -15 | LC_ALL=C sed 's/^/    /'
					BUILD_STATUS=1
				else
					printf '[PASS] %-21s pass=%s fail=%s invalid=%s total=%s (collections)\n' "EngineTest $cfg" "$pass" "$fail" "$invalid" "$total"
				fi
				echo "         full log: $log"
			done
		fi

		# Leave the tree as it was found. build.sh configures --fresh every time, so a
		# plain reconfigure is all that is needed to drop -D__TEST__ -D__UNIT_TEST__.
		if ! ./build.sh Applications/VulkanExample -dev >/dev/null 2>&1; then
			echo "  note: could not reconfigure without -test; run ./build.sh <target> -dev to restore"
		fi
	fi
fi

# ---------------------------------------------------------------------- verdict --
echo
echo "=========================================================================="
printf " mechanical violations : %d\n" "$VIOL"
printf " warnings / advisory   : %d\n" "$WARN"
if [[ ${#FILES[@]} -eq 0 ]]; then
	# Vacuity guard. With zero files in scope the lint checks above examined nothing
	# and a violation count of zero is meaningless — the same trap that let the
	# unit-test gate pass while running no tests. Say it plainly rather than let a
	# clean exit status be read as a style verdict.
	printf ' lint scope           : %s\n' 'NONE — no C/C++ sources in scope, so the lint checks above examined nothing and this exit status says nothing about style conformance.'
fi
printf " build gate            : %s\n" "$([[ $BUILD -eq 0 ]] && echo 'skipped (--no-build)' || ([[ $BUILD_STATUS -eq 0 ]] && echo "PASS ${BUILD_PASS}/${BUILD_TOTAL}" || echo 'FAIL'))"
echo "=========================================================================="
if [[ $BUILD_STATUS -ne 0 ]]; then exit 2; fi
if [[ $VIOL -ne 0 ]]; then exit 1; fi
exit 0
