# Plan — the two exceptions: dependent types (#27) and the concept body (#28)

Status: **awaiting approval.** Nothing below is applied until you say go.

Both defects are cases where the rule and the tooling disagree with something that cannot be
otherwise written, so the answer is not "fix the code" but "state the exception where a reviewer can
see it, and make the checker honour it". Both routes use a visible tool directive; neither adds prose
to an engine source beyond that directive.

## What each fix rests on (measured, not argued)

| Item | Evidence |
|---|---|
| #27 dependent type | A nested type that sizes itself with `MaxProvidersPerLane`, or an alias bounded by `MaxQueueSize`, fails to compile if it precedes the constant: `error: use of undeclared identifier 'MaxProvidersPerLane'` |
| #28 concept body | clang-format 22.1.8 wants `concept CLockable = requires(T t) {` on one line. `BraceWrapping.AfterConcept` does not exist: `error: unknown key 'AfterConcept'`. Wrapping the concept in `// clang-format off` / `// clang-format on` removes the complaint — proved on a scratch file where the concept was the only deviation |
| #28 side effect | `comments.py` currently reports both directive lines as `COMMENT` violations, so they need an exemption entry to be usable |

## Change 1 — the standard says a dependent type follows its constant

`docs/CodingStandards.md`, inserted after the *Safety invariant* bullets of the Member Ordering rule,
plus a matching clause in the `AGENTS.md` copy of the same rule:

```
    - **Exception: a type whose definition needs a class constant follows that constant.** Types head
      the class because they are neither state nor behaviour, but a nested type that sizes itself with
      `MaxProvidersPerLane`, or an alias that bounds itself with `MaxQueueSize`, cannot name a name
      the compiler has not seen yet — the order the rule wants does not compile. Put the directive
      `// hb-standards:ignore` on the type's own declaration line, keep the constant immediately above
      it, and say why in the module's design document: the directive is the marker a reviewer sees,
      the design document is where the reason belongs, because engine sources carry no prose.
```

## Change 2 — `layout.py` honours that directive instead of reporting it

`scripts/layout.py`: one helper, applied where findings are collected, so a waived finding is counted
separately rather than silently dropped.

```python
def directive_lines(text):
    """The 1-based line numbers that carry the visible exception directive."""
    return {index for index, line in enumerate(text.splitlines(), 1) if HB_IGNORE in line}
```

A finding is skipped when its `member_line` carries the directive, and the summary gains the count:

```
member layout: 46 file(s) checked (46 clean, 0 with findings), 0 skipped, 20 partial — 0 violation(s), 2 waived
```

Silence is not the same as a waiver, which is why the number is printed: a waived finding is still a
standing exception, and a report that hides it is the same mistake as a page that reads as complete.

## Change 3 — the directive goes on the two members

| File | Line | Declaration that carries `// hb-standards:ignore` |
|---|---|---|
| `Engine/Core/TaskStream.h` | 69 | `struct LaneProviders final` |
| `Engine/Core/MainThreadTaskQueue.h` | 59 | `using TQueue = BoundedPriorityQueue<TaskItem, 256, MaxQueueSize>;` |

No `TaskStream.cpp` change, so the pair rule is satisfied by the header alone; the two reasons go into
`docs/Core/TaskStream/index.html` (Coverage note) and the `MainThreadTaskQueue` page when it is written
(todo #16) — those pages are the only legal place for the explanation.

## Change 4 — the concept body, suppressed by the formatter itself

`check.sh` is not touched. clang-format has a native suppression mechanism, and using it means the
formatter and the gate agree without a custom rule in the checker.

1. `docs/CodingStandards.md` exemption list gains: the `// clang-format off` / `// clang-format on`
   pair is a tool directive, legal only around a construct the formatter cannot express, and must be
   the shortest possible span. `AGENTS.md` mirrors the one-line version.
2. `scripts/comments.py` adds `// clang-format off` and `// clang-format on` to the same exemption set
   that already carries `hb-standards:ignore`.
3. `Engine/Core/ScopedLock.h`: the pair wraps the `CLockable` concept only — 6 lines, not the file.

## Verification (all of it, not a sample)

| Step | Command | Pass condition |
|---|---|---|
| Selftests | `layout.py --selftest`, `comments.py --selftest` | exit 0, new directive cases included as fixtures |
| Core layout | `layout.py Engine/Core --only-violating` | 0 violations, 2 waived |
| Format, whole tree | `check.sh --all` | format layer PASS (was FAIL on 2 files) |
| Comment ban unchanged | `comments.py Engine/Core` | unchanged count except the ScopedLock directive lines no longer counted |
| No behaviour change | `check.sh --staged --test` | build gate `PASS 12/12`, EngineTest 59 collections in Dev, Debug, Release |

## Two decisions I want from you, not from myself

| Decision | Choice as proposed | The alternative |
|---|---|---|
| Reason with the directive? | **Bare directive** — `// hb-standards:ignore` with no explanation, reason lives in the design document. Keeps the ban's letter: no prose in engine sources. | `// hb-standards:ignore — sized by MaxProvidersPerLane` is friendlier to a reader at the line, but is prose in a source file and weakens the ban at exactly the boundary the ban exists to hold |
| `clang-format off/on` as an exemption class | **Accepted**, scoped to the shortest span and listed in the standard. It is the formatter's own mechanism, already in the same family as `hb-standards:ignore`. | Reformat `CLockable` to clang-format's shape, letting one construct break Allman; or leave the format gate red forever, which trains people to skim a red gate |

## Cost and risk

| Item | Cost | Risk |
|---|---|---|
| Change 1 | two paragraphs of documentation | the exception is real, so the risk is wording, not behaviour |
| Change 2 | ~10 lines in `layout.py` + fixtures | a directive placed on the wrong line would waive a genuine violation; the printed waived count is what keeps that visible |
| Change 3 | 2 source lines | none — a comment cannot change semantics, and both files rebuild |
| Change 4 | 6 lines in `ScopedLock.h`, ~4 in `comments.py`, one exemption bullet | someone wraps a whole file in `off/on` to dodge the formatter; the "shortest possible span" clause is the stated rule, and review is the enforcement |
