#!/usr/bin/env bash
# check.sh — format + strictly lint HardBop Engine sources touched by a commit,
# then prove the tree still builds in Debug, Dev and Release.
#
# Usage:
#   scripts/check.sh [--staged | <rev>] [--apply | --fix] [--no-build] [--test] [--all]
#
#   --staged     lint files staged for the next commit
#   <rev>        lint files touched by a revision (default: HEAD)
#   --apply      rewrite files with clang-format (default is check-only)
#   --fix        clang-format, then autofix.py, then clang-format, then re-lint
#   --no-build   skip the build gate
#   --test       also run the EngineTest suite for each configuration
#   --all        lint every source under Engine, Examples, Applications
#
# Exit status: 0 = clean, 1 = violations found, 2 = build failed, 3 = usage/environment error
#
# Layers, in the order they run. Each covers what the previous one structurally cannot:
#   1. clang-format        Allman braces, tabs, 120 columns, include order, blank-line *limits*.
#                          A pre-process, not the definition of clean: it deletes the second blank at
#                          the preamble seam, which rule A3 requires, so the gate compares through
#                          blank_lines.py --collapse-seam. See the seam note among the design notes.
#   2. mechanical greps    rules a formatter cannot express: joined empty bodies, no
#                          exceptions, m_ prefix, hygiene, include layout. `explicit inline` is
#                          advisory only: a grep cannot tell an in-class member (keyword is noise)
#                          from a header free function (keyword prevents a duplicate symbol).
#   2b. blank_lines.py     rule set A: the size of every blank-line seam clang-format leaves alone,
#         includes.py        and rule set B: the shape of the include preamble. Both report and never
#                          rewrite — which seam is a real paragraph, and which include is unused, are
#                          a reader's edits. scripts/blank_lines.py scripts/includes.py
#   3. comments.py         the comment ban, by lexing the file. scripts/comments.py
#   4. layout.py           the twelve-block member layout, from the clang AST.
#                          scripts/layout.py
#   5. docs_coverage.py    every declared entry owns a page under docs/, which is what makes
#                          deleting a comment safe. scripts/docs_coverage.py
#   6. build gate          Dev, Debug, Release, plus EngineTest on request.
# Layers 2b, 3, 4 and 5 report and never rewrite: no tool here can tell which comment belonged to
# which member, and reordering data members against each other changes C++ initialisation
# order. Those edits belong to a reader, and the layers exist to prove the reader worked.
#
# Design notes that are easy to get wrong, each verified on this tree:
#   * .mm / .m are EXCLUDED. clang-format classifies them as Objective-C, and the
#     repo .clang-format declares only `Language: Cpp`, so it aborts with
#     "Configuration file(s) do(es) not support Objective-C", exit 1, writing
#     nothing. A script that ignores exit status reports success while having
#     formatted none of them. Worse, run from a directory where the config is not
#     found, clang-format silently falls back to LLVM defaults and rewrites the
#     file — measured: 2249 -> 2400 bytes, wrong style, exit 0.
#   * .inl are EXCLUDED FROM FORMATTING ONLY. MatrixCommonImpl.inl and VectorCommonImpl.inl
#     are #include-d *inside a class body inside a namespace*, so their one-tab indentation
#     is inherited from the includer; formatting them standalone de-indents every line to
#     column 0. They are still comment-checked and layout-checked, because the members they
#     contribute belong to the class and the lines to fix live in the .inl — measured: the
#     Vector Lerp and Matrix CreateDiagonal ordering findings point into those two files.
#   * The build gate must not trust "ninja: no work to do". The touched files are
#     touched before building so a real recompile is forced and the gate cannot
#     pass vacuously.
#   * --test must build EngineTest with "build.sh ... -test". The body of main() and
#     every module's test bodies sit behind #ifdef TEST_ENABLED, so without the flag
#     the executable has an empty main and exits zero having run nothing (measured on
#     this tree: 0 tests without -test, 285 with it). Zero tests is a failure.
#   * Neither timeout(1) nor gtimeout(1) is guaranteed to exist, and on this machine
#     neither does. Calling one unconditionally exits 127 and the gate then blames the
#     code for a missing helper binary, so probe for both and run uncapped if absent.
#   * The blank lines around the include preamble are rule A3, owned by blank_lines.py, and clang-format
#     cannot express them: at the seam before a `namespace`, a `class` or a function definition it forces
#     exactly one blank however many a file holds, and `BreakAfterIncludes` is `error: unknown key` in
#     clang-format 22.1.8. Before rule A3, this position was governed by a retired convention (commit
#     ea0f157) that clang-format enforced alone — because a hand-written awk copy of it reported 85
#     findings the formatter disagreed with in 7. The position-dependent behaviour is real (two blanks
#     survive before a using-directive, one before a namespace) and is now carried by a lexer plus four
#     dozen fixtures rather than by a regex. Do not restore an awk copy of it.
#   * Member order is never checked by a pattern. C++ declarator syntax defeats regex exactly
#     where this rule lives: `std::function<void(int)> cb;` is data holding parentheses,
#     `using TLogFunc = std::function<void(std::ostream&)>;` is a type that reads as a call,
#     `explicit operator bool() const` is a function with no name, and an unnamed union is a
#     data member written in place, not a nested type. All four were mis-sorted by the first
#     prototype of layout.py, which is why it asks clang instead of guessing.
#   * The layout layer needs a compile database for the compiler's own flags. Absent, it says
#     [NONE] out loud: a rule that could not run must never be readable as a rule that passed.
#   * Empty scope is announced, not hidden. A commit touching no C++ yields zero
#     violations without proving anything, so the verdict states that the lint
#     examined nothing rather than falling silent.

set -uo pipefail

usage() { sed -n '2,16p' "$0"; exit 3; }

REVSPEC=""
STAGED=0
APPLY=0
FIX=0
BUILD=1
RUNTEST=0
ALL=0

while [[ $# -gt 0 ]]; do
	case "$1" in
		--staged)   STAGED=1 ;;
		--apply)    APPLY=1 ;;
		--fix)      FIX=1; APPLY=1 ;;
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
command -v python3 >/dev/null 2>&1 || { echo "python3 not on PATH — the comment, layout and docs-coverage checks cannot run" >&2; exit 3; }
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# The compile database the layout check reads clang's flags from. cmake exports it for the
# IDE configurations; build/ does not carry one, so cmake-build-debug is the canonical source.
COMPILE_DB="cmake-build-debug/compile_commands.json"

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
declare -a INL_FILES=()
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
		# .inl cannot be formatted standalone (its indentation is inherited from the class body
		# that #includes it) but its comments and member order are still the engine's business,
		# so it is collected for those checks instead of vanishing from the run.
		if [[ "$ext" == "inl" ]]; then INL_FILES+=("$f"); fi
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
# ---------------------------------------------------------------- scope fallback --
# A docs-only commit hides the code underneath it. An empty rev-scope then makes
# 'mechanical violations : 0' a statement about nothing, and --apply silently rewrites
# nothing either - which is how an unformatted header shipped once a plan commit landed
# on top of the commit that changed it. So an empty rev-scope re-enters this script on
# the newest ancestor that really did touch a source, exactly once: the marker keeps a
# scope whose files were all deleted from recursing.
if [[ ${#FILES[@]} -eq 0 && $ALL -eq 0 && $STAGED -eq 0 && -z "${HBE_CHECK_SCOPE_FALLBACK:-}" ]]; then
	ancestor=$(git log -1 --pretty=format:%H "${REVSPEC:-HEAD}" -- '*.h' '*.hpp' '*.cpp' '*.cc' 2>/dev/null)
	if [[ -n "$ancestor" && "$ancestor" != "$(git rev-parse "${REVSPEC:-HEAD}" 2>/dev/null)" ]]; then
		echo "scope fallback : ${REVSPEC} touches no C/C++ source - linting its code at ${ancestor:0:9} instead"
		export HBE_CHECK_SCOPE_FALLBACK=1
		exec "$0" "$ancestor" $( (( APPLY == 1 )) && printf -- '--apply' ) $( (( FIX == 1 )) && printf -- '--fix' ) $( (( BUILD == 0 )) && printf -- '--no-build' ) $( (( TEST == 1 )) && printf -- '--test' )
	fi
fi

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
# `VIOL` counts failing sections, one each, so before this split the verdict line added
# "files clang-format wants to rewrite" to "greps that failed" to "sections that reported backlog" and
# printed the sum as `mechanical violations`. A reader took 3 to mean three lines of bad code when it
# meant three sections still holding work the sweep has not reached. Backlog sections — comment ban,
# member layout, docs coverage — count separately now, because they are the remaining work, not
# violations of a rule that can be fixed today.
BACKLOG=0

# A --staged run is a promise about a commit, and a commit takes its bytes from the index.
# Every check below reads worktree files instead, so when the two differ the verdict does not
# describe the thing being committed. Observed directly: clang-format -i fixed the worktree, the
# shell chain that should have re-staged it died on a counting grep (grep -c prints 0 and exits
# 1, which breaks &&), and the commit recorded the unformatted file under an all-PASS lint.
# The divergence is the whole failure; detecting it here makes it impossible to miss.
if [[ $STAGED -eq 1 && ${#FILES[@]} -gt 0 ]]; then
	if ! git diff --quiet -- "${FILES[@]}" 2>/dev/null; then
		echo "[FAIL] index and worktree disagree on ${#FILES[@]} scoped file(s); the checks below read the worktree"
		git diff --name-only -- "${FILES[@]}" | sed 's/^/         differ: /'
		printf ' %s\n' '         A --staged commit takes bytes from the index, so a green verdict here would'
		printf ' %s\n' '         describe worktree content the commit never contains. Re-stage with'
		printf ' %s\n' '         git add -- <files>, then re-run this check.'
		VIOL=$((VIOL + 1))
	fi
fi

# A fix rewrites bytes. Doing it to a tree whose other edits belong to nobody in this run is
# how a formatter ends up committing someone else's half-finished refactor, so --fix refuses
# while changes exist outside the scope it was asked to fix.
if [[ $FIX -eq 1 && ${#FILES[@]} -gt 0 ]]; then
	printf '%s\n' "${FILES[@]}" | sort > "/tmp/hb_fix_scope.$$"
	git status --porcelain --untracked-files=no | sed 's/^...//' | cut -d' ' -f1 | sort > "/tmp/hb_fix_dirty.$$"
	# Only the languages a fix rewrites can block it. The first version compared every dirty
	# path, which meant the tool refused to run while its own scripts were being edited — a
	# guard that blocks its own development is a guard nobody keeps.
	comm -23 "/tmp/hb_fix_dirty.$$" "/tmp/hb_fix_scope.$$" \
		| grep -E '\.(h|hh|hpp|cpp|cc|cxx)$' > "/tmp/hb_fix_outside.$$" || true
	outside=$(cat "/tmp/hb_fix_outside.$$")
	rm -f "/tmp/hb_fix_scope.$$" "/tmp/hb_fix_dirty.$$" "/tmp/hb_fix_outside.$$"
	if [[ -n "$outside" ]]; then
		echo "[FAIL] --fix edits the scoped file(s) only; these are modified outside that scope:"
		printf '         %s\n' $outside
		printf ' %s\n' '         Commit, stash, or widen the scope first. A fix that sweeps unrelated'
		printf ' %s\n' '         work into its own commit cannot be proved by prove_format.py afterwards.'
		exit 3
	fi
fi

# Belt and braces: clang-format enforces the Allman brace rule, the 120 column
# limit, tabs, and the blank-line conventions. Everything after this section
# covers rules clang-format structurally cannot check.
if [[ ${#FILES[@]} -gt 0 ]]; then
	if [[ $APPLY -eq 1 ]]; then
		clang-format --style=file -i "${FILES[@]}" || { echo "clang-format --apply failed" >&2; exit 1; }
		# The mechanical half of a fix, then the formatter again so the braces the split just
		# opened land on the project's own columns. autofix declares every token it removes, and
		# the manifest it writes is what prove_format.py later holds this commit to.
		if [[ $FIX -eq 1 ]]; then
			python3 "$SCRIPT_DIR/autofix.py" --manifest .Plans/fix-manifest.json "${FILES[@]}"
			clang-format --style=file -i "${FILES[@]}" \
				|| { echo "clang-format --apply failed after autofix" >&2; exit 1; }
			echo "[FIX] mechanical fixes applied to ${#FILES[@]} file(s); manifest .Plans/fix-manifest.json"
		fi
		# clang-format -i writes the worktree and does not touch the index, so with --staged
		# scope the index keeps the bytes the formatter just rejected while every check below
		# reads the formatted worktree and calls them clean. The commit then records the
		# unformatted file under a green lint. Measured before this line existed: lint
		# reported all-PASS while the index still held
		# "namespace hbe {<tab>constexpr int reformatProbe=1+2;}".
		# Re-stage exactly the files rewritten here. --all and <rev> scopes must not reach
		# the index: staging there sweeps unrelated work into the commit, which is the
		# failure mode this script exists to prevent.
		if [[ $STAGED -eq 1 ]]; then
			git add -- "${FILES[@]}" || { echo "git add failed after --apply" >&2; exit 1; }
			echo "[PASS] clang-format applied to ${#FILES[@]} file(s), re-staged"
		else
			echo "[PASS] clang-format applied to ${#FILES[@]} file(s)"
		fi
	else
		cfbad=0
		cffail=""
		for f in "${FILES[@]}"; do
			# Both sides pass through `blank_lines.py --collapse-seam`, which reduces rule A3's sanctioned
			# double blank to the single blank clang-format produces. Without it the gate compares a file
			# against a shape the standard forbids: the formatter deletes that second blank before a
			# namespace, a class or a function definition, so every conforming file would differ from its
			# own formatted form by one line. Measured: the raw diff says 1 line for such a file, the
			# seam-aware diff says 0, and a K&R-braced file still reports 8. This tolerates that one
			# position and nothing else - an A16 double blank still shows up as a diff.
			d=$(diff <(clang-format --style=file "$f" 2>/dev/null | python3 "$SCRIPT_DIR/blank_lines.py" --collapse-seam) \
					<(python3 "$SCRIPT_DIR/blank_lines.py" --collapse-seam "$f" 2>/dev/null) 2>/dev/null | grep -c '^[<>]' || true)
			if [[ "${d:-0}" -gt 0 ]]; then
				cfbad=$((cfbad+1))
				cffail+="    $f  ($d line(s) differ — run with --apply then redo the blank-line pass)"$'\n'
			fi
		done
		if [[ $cfbad -eq 0 ]]; then
			echo "[PASS] clang-format — Allman braces, tabs, 120 columns, blank-line limits (rule A3's seam excepted)"
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
	check "joined empty record"                '(struct|class|union|enum)[[:space:]]+[[:alnum:]_]+[[:space:]]*\{[[:space:]]*\}[[:space:]]*;' FAIL "${FILES[@]}"
	check "no exceptions (engine is exception-free)" '\b(throw\s+[A-Za-z_(]|try\s*\{|catch\s*\()' FAIL "${FILES[@]}"
	# The banned thing is the prefix itself. A second alternative here used to be
	# `[a-z]+_[a-z]+\s*;`, "a snake_case name before a semicolon", which matched `using TValue =
	# size_t;` and every other fixed-width type at a line end — 1 false file in Config alone. A
	# snake_case member is a different rule and needs the type in front of it to be recognised as a
	# declaration, so it is reported separately rather than folded into this grep.
	check "no m_ member prefix"                '\bm_[a-zA-Z]'                                      FAIL "${FILES[@]}"
	check "std::move on return kills NRVO"     'return\s+std::move'                              FAIL "${FILES[@]}"
	check "virtual alongside override"         'virtual\s+[^;{]*\boverride'                      FAIL "${FILES[@]}"

	hdr "naming and interface conventions"
	# `inline` cannot be policed by a grep, and a FAIL here would order something that breaks the build.
	# 16 of the 18 sites in this tree are functions or operators defined at namespace scope in a header,
	# where the keyword is what stops every including translation unit emitting a duplicate symbol;
	# only in-class member definitions are noise. Distinguishing them needs the AST, so the rule lives
	# in SKILL.md's manual list and this line reports without failing.
	check "redundant inline keyword?"          '^\s*inline\s+[A-Za-z_]'                          WARN "${FILES[@]}"
	# A snake_case member name cannot be grepped: the member name is the token before `;`, and
	# `size_t MaxNameLength = 127;` has a type where a snake_case member would be. An attempt at
	# this rule flagged every size_t declaration in the tree, which is worse than not checking it. It is
	# a manual review item, listed in SKILL.md alongside the other rules a grep cannot hold.
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
		# LC_ALL=C pins the collation of the string compares below to byte order.
		# Without it `L[i] < last` follows the ambient locale, so the verdict depends on
		# the developer's machine: under a en_US-style collation "Logger.h" sorts before
		# "LogLevel.h" (case folded, then 'g' < 'l'), while clang-format sorts
		# case-sensitively and puts "LogLevel.h" first — the two lint layers then demand
		# opposite orders of the same include block and no ordering can satisfy both
		# (measured on Engine/Engine/Engine.h). Byte order is what clang-format uses, so
		# this makes layer 2 agree with layer 1 AND makes the check reproducible. Scoped to
		# this awk program rather than the script, so no other check changes behaviour.
		out=$(LC_ALL=C awk -v F="$f" -v OWN="$own" '
			# phase tracks whether the top-of-file include preamble is still open. The
			# dominant repo idiom puts further #include directives inside a
			# "#ifdef TEST_ENABLED" test block far below the first code body, so the
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
			# in this repo that is almost always the "#ifdef TEST_ENABLED" test block or a
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
				# Category per line, kept alongside the path: ordering is only meaningful
				# inside a category. A block that jams <...> and "..." together has a missing
				# blank line, which is a defect clang-format already reports by name, and
				# comparing "chrono" against "LogLevel.h" on top of it reports the same file
				# twice for reasons that read as unrelated.
				listk[block] = listk[block] (($0 ~ /^[ \t]*#[ \t]*include[ \t]*</) ? "std" : "proj") "\n"
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

				# Alphabetical within each category of each block. Comparing across the
				# <...>/"..." boundary is meaningless - they are separate categories, and a
				# block containing both is a blank-line defect, not an ordering defect - so the
				# last item of each kind is tracked on its own.
				for (b = 1; b <= nb; b++) {
					m = split(list[b], L, "\n"); split(listk[b], K, "\n")
					laststd = ""; lastproj = ""; unsorted = 0
					for (i = 1; i <= m; i++) {
						if (L[i] == "") continue
						if (K[i] == "std") {
							if (laststd != "" && L[i] < laststd) unsorted = 1
							laststd = L[i]
						} else {
							if (lastproj != "" && L[i] < lastproj) unsorted = 1
							lastproj = L[i]
						}
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

	hdr "comment ban — no comments in .h or .cpp, docs/ holds the prose"
	# A lexer, not a grep: `grep '//'` calls "http://" inside a string literal a comment.
	# Exemptions are exhaustive and listed in docs/CodingStandards.md: the line-1 copyright,
	# a structural label on the line that closes its construct, `hb-standards:ignore`, and
	# Engine/CodingStandards.cpp, which teaches the rule by breaking it.
	declare -a COMMENT_TARGETS=("${FILES[@]}")
	[[ ${#INL_FILES[@]} -gt 0 ]] && COMMENT_TARGETS+=("${INL_FILES[@]}")
	comment_out=$(python3 "$SCRIPT_DIR/comments.py" "${COMMENT_TARGETS[@]}" 2>&1)
	comment_code=$?
	if [[ $comment_code -eq 0 ]]; then
		echo "[PASS] no comment outside the exemption list"
	else
		echo "$comment_out" | sed 's/^/    /'
		BACKLOG=$((BACKLOG+1))
	fi

	hdr "member layout — twelve blocks: types, then all data, then all functions"
	# Read from the clang AST, not a pattern: `std::function<void(int)> cb;` is data that
	# contains parentheses and `explicit operator bool() const` is a function with no name.
	if [[ -f $COMPILE_DB ]]; then
		layout_out=$(python3 "$SCRIPT_DIR/layout.py" --quiet --db "$COMPILE_DB" "${FILES[@]}" 2>&1)
		layout_code=$?
		echo "$layout_out" | grep -vE '^\[|member layout:' | sed 's/^/    /'
		echo "  $(echo "$layout_out" | grep 'member layout:')"
		echo "$layout_out" | grep -E '^\[[A-Z]+\]' | sed 's/^/    note: /'
		if [[ $layout_code -ne 0 ]]; then BACKLOG=$((BACKLOG+1)); fi
	else
		printf '[NONE] member layout — %s is absent; configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON.\n' "$COMPILE_DB"
		printf '       An unmeasured rule is not a passing rule, so this is stated, not skipped.\n'
	fi

	hdr "docs coverage — every declared entry owns a page, every method a page, and every page is valid"
	# The comment ban moves each contract into the HTML reference. Deleting a comment whose
	# page does not exist yet destroys the only copy, so this gate is what makes that safe.
	# Three checks, because "a page exists" is three different claims: the class owns one
	# (docs_coverage), each declared method has one of its own (docs_methods), and the pages
	# that do exist are valid HTML with links that resolve (htmlcheck). Only the first was
	# wired here, so a module could pass layer 5 with 18 method pages that covered none of its
	# methods and a sidebar full of dead links — which is exactly what happened to Log.
	modules_touched=$(for f in "${FILES[@]}"; do [[ "$f" == Engine/*/* ]] && printf '%s\n' "${f#Engine/}" | cut -d/ -f1; done | sort -u)
	coverage_out=$(python3 "$SCRIPT_DIR/docs_coverage.py" check $modules_touched 2>&1)
	coverage_code=$?
	echo "$coverage_out" | sed 's/^/    /'
	if [[ $coverage_code -ne 0 ]]; then BACKLOG=$((BACKLOG+1)); fi

	if [[ -n "$modules_touched" ]]; then
		methods_out=$(python3 "$SCRIPT_DIR/docs_methods.py" $modules_touched 2>&1)
		methods_code=$?
		echo "$methods_out" | sed 's/^/    /'
		if [[ $methods_code -ne 0 ]]; then BACKLOG=$((BACKLOG+1)); fi

		doc_pages=()
		for m in $modules_touched; do
			while IFS= read -r page; do doc_pages+=("$page"); done < <(find "docs/$m" -name '*.html' 2>/dev/null | sort)
		done
		if [[ ${#doc_pages[@]} -gt 0 ]]; then
			html_out=$(python3 "$SCRIPT_DIR/htmlcheck.py" "${doc_pages[@]}" 2>&1)
			html_code=$?
			echo "$html_out" | sed 's/^/    /'
			# A page that exists and is broken is a defect, not remaining work, so this one is a
			# violation rather than backlog: an undeclared CSS class or a dead anchor is a bug in
			# what was just written, and the reader meets it before anyone reads the ledger.
			if [[ $html_code -ne 0 ]]; then VIOL=$((VIOL+1)); fi
		fi
	fi

	hdr "convention debt — namespace indentation, reported while the sweep is in progress"
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

# ---------------------------------- blank-line paragraphs and the include preamble --
hdr "blank lines (rule set A) and the include preamble (rule set B)"
# Layer 2b. Rule set A sizes the seams clang-format leaves alone; rule set B shapes the preamble. Neither
# rewrites anything: which seam is a real paragraph (A8, A9, A11) is a reader's judgement, and an include
# deletion is a build-gated edit whose consumer may live in another module. Skill layer 2b owns the prose.
if [[ ${#FILES[@]} -gt 0 ]]; then
	bl_out=$(python3 "$SCRIPT_DIR/blank_lines.py" "${FILES[@]}" 2>&1); bl_code=$?
	inc_out=$(python3 "$SCRIPT_DIR/includes.py" "${FILES[@]}" 2>&1); inc_code=$?
	bl_sum=$(printf '%s\n' "$bl_out" | grep '^blank lines: ' || true)
	inc_sum=$(printf '%s\n' "$inc_out" | grep '^includes: ' || true)
	if [[ $bl_code -eq 0 && $inc_code -eq 0 ]]; then
		printf '%s\n%s\n' "$bl_sum" "$inc_sum"
	elif [[ $bl_code -eq 3 || $inc_code -eq 3 ]]; then
		printf '%s\n%s\n' "$bl_out" "$inc_out"
		echo "         [NONE] a checker could not run on this scope; an unmeasured rule is not a passing rule"
	elif [[ $ALL -eq 1 ]]; then
		printf '%s\n%s\n' "$bl_sum" "$inc_sum"
		echo "         [DEBT] reported, not gated, while the sweep is an open owner decision: the tree predates"
		echo "         both rule sets, so failing every commit that touches an engine file would block"
		echo "         unrelated work — the failure mode this script exists to prevent. The same findings are"
		echo "         violations in a scoped run (a revision, or --staged), which is where the rules bite."
	else
		printf '%s\n%s\n' "$bl_out" "$inc_out"
		echo "[FAIL] rule set A and/or rule set B — docs/CodingStandards.md holds the tables, layer 2b the method"
		VIOL=$((VIOL+1))
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
		# #ifdef TEST_ENABLED, so a binary built without the flag has an EMPTY main: it
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
		# plain reconfigure is all that is needed to drop -DTEST_ENABLED -DTEST_ENABLED.
		if ! ./build.sh Applications/VulkanExample -dev >/dev/null 2>&1; then
			echo "  note: could not reconfigure without -test; run ./build.sh <target> -dev to restore"
		fi
	fi
fi

# ---------------------------------------------------------------------- verdict --
echo
echo "=========================================================================="
printf " grep rule failures   : %d\n" "$VIOL"
printf " advisory             : %d\n" "$WARN"
printf " sweep backlog        : %d%s\n" "$BACKLOG" "$([[ $BACKLOG -gt 0 ]] && echo '  (comment ban, member layout, docs coverage — see above)')"
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
if [[ $VIOL -ne 0 || $BACKLOG -ne 0 ]]; then exit 1; fi
exit 0
