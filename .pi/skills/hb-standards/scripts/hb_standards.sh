#!/usr/bin/env bash
# hb_standards.sh — format + strictly lint HardBop Engine sources touched by a commit,
# then prove the tree still builds in Debug, Dev and Release.
#
# Usage:
#   hb_standards.sh [--staged | <rev>] [--apply] [--no-build] [--test] [--all]
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
		SKIPPED+=("$f ($ext — excluded: see hb_standards.sh header)")
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
	[[ $BUILD -eq 1 ]] || exit 0
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
			/^#(include|define)/ {
				if (gap && count) { block++ ; gap = 0 }                     # a blank line opens a new block
				if (block == "") block = 1
				if ($0 ~ /^#include </) kind[block] = "std"
				else                    kind[block] = "proj"
				path = $0
				sub(/^[ \t]*#[ \t]*(include|define)[ \t]*/, "", path)
				sub(/^["<]/, "", path)
				sub(/[">].*$/, "", path)          # cut at the FIRST closer, not the last
				list[block] = list[block] path "\n"
				if (block == 1 && firstinc == "") firstinc = path
				count++
				blank=0
				next
			}
			/^$/ { if (count && !bodyline) { gap = 1; blank++ } ; next }
			{ if (count) { bodyline = NR } ; next }
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
				if (bodyline && nb > 0 && blank != 1) print "expected exactly one empty line before the first code body (found " blank ") — clang-format collapses any other count"
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
		printf '[DEBT] %d file%s indent%s namespace bodies; the standard says NamespaceIndentation: None\n' "$nsi" "$([[ $nsi -eq 1 ]] && echo '' || echo 's')" "$([[ $nsi -eq 1 ]] && echo 's' || echo '')"
		echo "         left as-is on purpose: flipping this is a ~14k-line repository decision"
	else
		echo "[PASS] namespace bodies not indented"
	fi
fi

# ------------------------------------------------------------------ build gate --
# Runs last and is mandatory unless --no-build. Proves the reformat did not break
# compilation in any of the three project configurations.
BUILD_STATUS=0
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
				vacuous=""
				grep -q "no work to do" <<<"$out" && vacuous="  (up to date — no recompile exercised)"
				printf '[PASS] %-12s %-8s%s\n' "$t" "$name" "$vacuous"
			else
				printf '[FAIL] %-12s %-8s\n' "$t" "$name"
				grep -E 'error:|FAILED' <<<"$out" | head -15 | sed 's/^/    /'
				BUILD_STATUS=1
			fi
		done
	done

	if [[ $RUNTEST -eq 1 && $BUILD_STATUS -eq 0 ]]; then
		hdr "unit tests"
		for cfg in Dev Debug Release; do
			bin="./build/Applications/EngineTest/$cfg/EngineTest"
			if [[ -x "$bin" ]]; then
				if timeout 120 "$bin" >/tmp/hb_test.$cfg 2>&1; then
					printf '[PASS] EngineTest %-8s %s\n' "$cfg" "$(grep -c 'PASS' /tmp/hb_test.$cfg) pass lines"
				else
					printf '[FAIL] EngineTest %s\n' "$cfg"; tail -20 /tmp/hb_test.$cfg | sed 's/^/    /'; BUILD_STATUS=1
				fi
			else
				printf '[WARN] EngineTest %-8s binary not built: %s\n' "$cfg" "$bin"
			fi
		done
	fi
fi

# ---------------------------------------------------------------------- verdict --
echo
echo "=========================================================================="
printf " mechanical violations : %d\n" "$VIOL"
printf " warnings / advisory   : %d\n" "$WARN"
printf " build gate            : %s\n" "$([[ $BUILD -eq 0 ]] && echo 'skipped (--no-build)' || ([[ $BUILD_STATUS -eq 0 ]] && echo 'PASS 9/9' || echo 'FAIL'))"
echo "=========================================================================="
if [[ $BUILD_STATUS -ne 0 ]]; then exit 2; fi
if [[ $VIOL -ne 0 ]]; then exit 1; fi
exit 0
