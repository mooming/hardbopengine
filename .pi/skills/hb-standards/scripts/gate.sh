#!/bin/bash
#
# gate.sh — run the hb-standards gate in a SEPARATE PROCESS so the calling
# assistant keeps its turn instead of blocking for minutes on the build.
#
# Usage:
#   scripts/gate.sh spawn [--all|--staged|<rev>] [--apply] [--test] [--no-build]
#                         [--provider <name>] [--model <pattern>] [--thinking <level>]
#   scripts/gate.sh status [<job>]      # RUNNING / DONE + the gate verdict
#   scripts/gate.sh wait   [<job>]      # block, then exit with the gate's verdict
#   scripts/gate.sh list                # every job, its state, and slot occupancy
#   scripts/gate.sh release             # drop slots left behind by a killed job
#
# A job is a directory under .pi/logs/gate/ holding:
#   cmd       the exact check.sh invocation
#   pi.log    the detached assistant's full transcript
#   done      written when that process exits; holds pi's own exit code
#   GATE_EXIT=<n> appears inside pi.log once check.sh has run — that, not the
#   process exit code, is the verdict.
#
# Concurrency: at most ${PI_GATE_SLOTS:-4} jobs at a time. The build tree is
# shared, so this is not a courtesy limit — two gates racing on cmake-build-*
# produce results that mean nothing. Ask the owner before raising it.
#
# Exit status mirrors check.sh: 0 clean, 1 violations, 2 build failed,
# 3 usage/environment error.

set -u

SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "$SKILL_DIR/../../.." && pwd)"
LOGROOT="$REPO/.pi/logs/gate"
SLOTS="${LOGROOT}/slots"
SLOTCAP="${PI_GATE_SLOTS:-4}"

usage() { sed -n '2,27p' "${BASH_SOURCE[0]}" | sed 's|^# \{0,1\}||'; }

latest_job() { ls -1d "$LOGROOT"/[0-9]*/ 2>/dev/null | sed 's:/$::' | sort | tail -1; }

resolve_job() {
    local j="${1:-}"
    if [ -z "$j" ]; then
        j="$(latest_job)"
        [ -n "$j" ] || { echo "gate.sh: no jobs under $LOGROOT" >&2; return 3; }
    elif [ ! -d "$j" ]; then
        j="$(ls -1d "$LOGROOT"/*"$j"*/ 2>/dev/null | sed 's:/$::' | sort | tail -1)"
        [ -n "$j" ] || { echo "gate.sh: no job matching '$1'" >&2; return 3; }
    fi
    printf '%s' "$j"
}

job_state() {
    local d="$1" code gate verdict pad=" "
    if [ -f "$d/done" ]; then
        code="$(cat "$d/done" 2>/dev/null || echo '?')"
        gate="$(grep -o 'GATE_EXIT=[0-9]*' "$d/pi.log" 2>/dev/null | tail -1 | cut -d= -f2)"
        case "${gate:-}" in
            0) verdict="CLEAN" ;;
            1) verdict="VIOLATIONS" ;;
            2) verdict="BUILD-FAILED" ;;
            3) verdict="USAGE-ERROR" ;;
            *) verdict="NO-VERDICT(pi=$code)" ;;
        esac
    else
        verdict="RUNNING"; pad="  "
    fi
    printf '%s %-15s gate_exit=%-3s %s\n' "$pad" "$verdict" "${gate:--}" "$d"
}

held_slots() { ls -1d "$SLOTS"/slot-*/ 2>/dev/null | sed 's:/$::'; }

mkdir -p "$LOGROOT" "$SLOTS"

case "${1:-}" in
    status)
        shift; d="$(resolve_job "${1:-}")" || exit 3; job_state "$d"; exit 0 ;;
    wait)
        shift; d="$(resolve_job "${1:-}")" || exit 3
        while [ ! -f "$d/done" ]; do sleep 3; done
        job_state "$d"
        gate="$(grep -o 'GATE_EXIT=[0-9]*' "$d/pi.log" 2>/dev/null | tail -1 | cut -d= -f2)"
        if [ -n "${gate:-}" ]; then exit "$gate"; fi
        exit "$(cat "$d/done")" ;;
    list)
        n="$(held_slots | wc -l | tr -d ' ')"; echo "slots held: $n/$SLOTCAP"
        for s in $(held_slots); do printf '  %s -> %s\n' "$(basename "$s")" "$(cat "$s/owner" 2>/dev/null || echo '?')"; done
        shopt -s nullglob
        for d in "$LOGROOT"/[0-9]*/; do job_state "${d%/}"; done
        exit 0 ;;
    release)
        for s in $(held_slots); do
            j="$(cat "$s/owner" 2>/dev/null || true)"
            if [ -z "$j" ] || [ -f "$j/done" ]; then
                rm -rf "$s"; echo "gate.sh: released $(basename "$s")"
            else
                echo "gate.sh: kept  $(basename "$s") (job $j still running)"
            fi
        done
        exit 0 ;;
    spawn|"")
        shift || true ;;
    -h|--help|help)
        usage; exit 0 ;;
    *)
        echo "gate.sh: unknown subcommand '$1' (spawn|status|wait|list|release)" >&2
        usage >&2; exit 3 ;;
esac

PROVIDER="${PI_PROVIDER:-}"; MODEL="${PI_MODEL:-}"; THINKING="medium"
TOOLS="read,bash,grep,find,ls"; APPLY=0; ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --provider) PROVIDER="${2:-}"; shift 2 ;;
        --model)    MODEL="${2:-}"; shift 2 ;;
        --thinking) THINKING="${2:-}"; shift 2 ;;
        --apply)    APPLY=1; ARGS+=("$1"); shift ;;
        *)          ARGS+=("$1"); shift ;;
    esac
done
[ "$APPLY" -eq 1 ] && TOOLS="read,bash,edit,write,grep,find,ls"

INVOCATION="$SKILL_DIR/scripts/check.sh ${ARGS[*]-}"

command -v pi >/dev/null 2>&1 || { echo "gate.sh: 'pi' not on PATH" >&2; exit 3; }
if [ -z "$PROVIDER" ] || [ -z "$MODEL" ]; then
    echo "gate.sh: no model pinned. A detached pi picks its own default provider and" >&2
    echo "         dies with '401 Invalid bearer token' at startup. Pass" >&2
    echo "         --provider/--model, or run where PI_PROVIDER and PI_MODEL are set" >&2
    echo "         (they are, inside a pi session)." >&2
    exit 3
fi

# One cheap roundtrip beats a 401 buried in a log nobody reads until the end.
PREFLIGHT="$(pi -p --offline --no-session --no-tools --provider "$PROVIDER" \
    --model "$MODEL" "Reply with exactly: OK" 2>&1 | tail -1)"
case "$PREFLIGHT" in
    *OK*) ;;
    *) echo "gate.sh: credential preflight failed for $PROVIDER/$MODEL:" >&2
       echo "         $PREFLIGHT" >&2; exit 3 ;;
esac

HELD="$(held_slots | wc -l | tr -d ' ')"
if [ "$HELD" -ge "$SLOTCAP" ]; then
    echo "gate.sh: all $SLOTCAP slots are held. The build tree is shared, so a fifth" >&2
    echo "         gate would race on cmake-build-* and prove nothing." >&2
    echo "         scripts/gate.sh list      # who holds them" >&2
    echo "         scripts/gate.sh release   # drop slots whose job already finished" >&2
    exit 3
fi
SLOT=""
for i in $(seq 0 $((SLOTCAP - 1))); do
    if mkdir "$SLOTS/slot-$i" 2>/dev/null; then SLOT="$SLOTS/slot-$i"; break; fi
done
if [ -z "$SLOT" ]; then
    echo "gate.sh: every slot was taken between count and acquire; try again." >&2
    exit 3
fi

JOB="$LOGROOT/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$JOB"
printf '%s\n' "$INVOCATION" > "$JOB/cmd"
printf '%s\n' "$JOB" > "$SLOT/owner"

if [ "$APPLY" -eq 1 ]; then
    SCOPE="An --apply was requested: you may rewrite formatting in the files this gate covers, and nothing else."
else
    SCOPE="Treat every violation as information only: do not edit, create, delete, format, or commit any file."
fi

PROMPT="Run the HardBop Engine coding-standards gate now, in this repository, exactly as the hb-standards skill directs.

Steps, in order, with no substitutions:
1. Run this one command with the bash tool and wait for it to finish:
       $INVOCATION; echo \"GATE_EXIT=\$?\"
   The GATE_EXIT line is how the parent process reads the verdict, so it must appear in your output verbatim.
2. Report per the skill's Reporting section: violation count per layer, the per-configuration build result, and anything left as [DEBT] or [WARN]. Quote real lines from the output; never infer a pass.
3. $SCOPE Never push.
4. Never claim the build gate passed on the strength of a \"no work to do\" build; the skill explains why.
5. Do not launch subagents or workflows; you are the detached worker already."

nohup env PI_GATE_JOB="$JOB" PI_GATE_SLOT="$SLOT" bash -c '
    pi -p --offline --no-session --approve --skill '"'"$SKILL_DIR"'"' \
       --provider "'"$PROVIDER"'" --model "'"$MODEL"'" \
       --tools "'"$TOOLS"'" --thinking "'"$THINKING"'" --name hb-gate \
       "$2" > "$PI_GATE_JOB/pi.log" 2>&1
    echo $? > "$PI_GATE_JOB/done"
    rm -rf "$PI_GATE_SLOT"
' _ "$JOB" "$PROMPT" > /dev/null 2>&1 &

CHILD=$!
echo "$CHILD" > "$JOB/pid"

echo "gate.sh: detached gate started (pid $CHILD, slot $(basename "$SLOT"), $(($(held_slots | wc -l | tr -d ' ')))/$SLOTCAP held)"
echo "  job      $(basename "$JOB")"
echo "  command  $INVOCATION"
echo "  model    $PROVIDER/$MODEL"
echo "  log      $JOB/pi.log"
echo "  follow   tail -f $JOB/pi.log"
echo "  verdict  .pi/skills/hb-standards/scripts/gate.sh status $(basename "$JOB")"
echo "  blocking .pi/skills/hb-standards/scripts/gate.sh wait   $(basename "$JOB")"
