export const meta = {
  name: 'hb-review-citation-gated',
  description: 'Per-file hb-standards judgement review, citations machine-verified, 3 concurrent',
  whenToUse: 'Owner asks for a per-file standards review and wants it off the main session.',
  phases: [
    { title: 'Review', detail: '14 batches x 20 files, findings gated by verify-findings.py' },
  ],
}

const SUMMARY = {
  type: 'object',
  additionalProperties: false,
  required: ['batch', 'filesAssigned', 'filesRead', 'findingsWritten', 'path'],
  properties: {
    batch: { type: 'integer' },
    filesAssigned: { type: 'integer' },
    filesRead: { type: 'integer', description: 'files actually opened in full' },
    findingsWritten: { type: 'integer' },
    path: { type: 'string', description: 'the JSON file written' },
    notes: { type: 'string', description: 'anything not read, stated plainly; else empty' },
  },
}

const RULES = `RULES TO CHECK (formatting/include-order/hygiene are already handled by check.sh; these need judgement):
1. Log-before-failing: an early return / Assert on an error path must be preceded by a descriptive Error/Warning log.
2. [[nodiscard]] on getters and on functions whose result must not be discarded.
3. explicit on every single-argument constructor.
4. "= default" instead of "{}" for trivial special members.
5. Parameter naming: "out" write-only ref, "inOut" read-write ref, "in" when it would shadow a member.
6. Pointer validated before dereference; references preferred over raw pointers.
7. static_assert wherever a condition is compile-time evaluable.
8. No magic numbers; named constants are PascalCase/constexpr.
9. Composition over inheritance; "final" on classes not designed as bases.
10. noexcept only where provably exception-free (not where allocation happens).
11. No m_ prefix, no Hungarian notation; members camelCase; functions and types PascalCase.
12. An "#ifdef __UNIT_TEST__" region must be at the END of the file, one per file.`

const SKIP = `DO NOT REPORT (already tracked; reporting them wastes the run):
- Anything clang-format fixes: braces, indentation, spacing, blank lines, include order.
- Presence/absence of comments in .cpp files (a separate migration is scheduled).
- Namespace bodies indented at column 1 (owner-confirmed legacy debt).
- Speculative refactors, performance opinions, "consider renaming". Only the 12 rules above.`

const TOTAL = 276, SIZE = 20, WIDTH = 3

phase('Review')
const batches = []
for (let i = 0; i < Math.ceil(TOTAL / SIZE); i++) batches.push(i)
log(`${batches.length} batches x ${SIZE} files, ${WIDTH} at a time (owner cap), citations gated`)

const results = []
for (let c = 0; c < batches.length; c += WIDTH) {
  const wave = batches.slice(c, c + WIDTH)
  const out = await parallel(wave.map((i) => () => {
    const start = i * SIZE + 1
    const end = Math.min(start + SIZE - 1, TOTAL)
    const out_path = `.Plans/review/batch${String(i).padStart(2, '0')}.json`
    const prompt = `You are auditing one slice of the HardBop Engine C++ codebase (repo root: /Users/anav/atelier/hardbopengine) against its coding standards.

Your files, and ONLY these. Run exactly this command, with the pathspecs intact - the tree contains docs and vendored External/ sources that are NOT yours to review:
    git ls-files '*.h' '*.cpp' '*.inl' '*.mm' | sed -n '${start},${end}p'

That is ${end - start + 1} files. Open EVERY one of them in full with the read tool. Rules 1, 4, 6 and 9 are invisible in excerpts, so a grep-only pass is not a review.

${RULES}

${SKIP}

Write your findings as JSON to ${out_path} using the write tool, shaped exactly:
    {"batch": ${i}, "filesRead": <int>, "findings": [{"file":"<repo-relative path>","line":<int>,"rule":"short name","evidence":"<verbatim source text from that line>","why":"one sentence","fix":"the concrete edit","confidence":"high|medium|low"}]}

ZERO findings is a correct and welcome answer for a clean slice. Write {"findings": []} and be done.

Hard rules, because a previous run of this task invented defects:
- A validator will check every citation: the file must exist, the line must be within it, and your "evidence" must appear verbatim on or beside that line. Anything that fails marks this batch FAILED and you will have to redo it.
- Therefore never report a line you have not read, and never write from memory of a filename. If you cannot quote it, drop it.
- Report at most 10 findings, highest confidence first.
- Exempt from behavioural rules: Engine/CodingStandards.h and .cpp (deliberate BAD EXAMPLEs); Applications/EngineTest/TestMain.cpp (its __UNIT_TEST__ guard is intentionally main()'s body).
- Read-only otherwise: do not edit, create, or delete any engine source, do not run clang-format, builds, tests, or check.sh, and never push. The only file you may write is ${out_path}.
- Do not launch subagents or workflows.

Your structured return is a summary, not the findings: report batch ${i}, how many files you were assigned, how many you actually opened in full, how many findings you wrote, the path, and in "notes" name any assigned file you did NOT open.`
    return agent(prompt, {
      label: `review:${start}-${end}`,
      phase: 'Review',
      schema: SUMMARY,
      effort: 'medium',
      gate: `python3 .pi/skills/hb-standards/scripts/verify-findings.py ${out_path}`,
    })
  }))
  results.push(...out)
  const failed = out.filter((x) => x === null).length
  if (failed) log(`wave ${c / WIDTH + 1}: ${failed} batch(es) failed the citation gate - reported, not retried`)
}

const ok = results.filter(Boolean)
log(`${ok.length}/${batches.length} batches passed the gate; ${results.length - ok.length} failed or died`)
return {
  batchesTotal: batches.length,
  batchesPassed: ok.length,
  filesAssigned: ok.reduce((n, r) => n + (r.filesAssigned || 0), 0),
  filesRead: ok.reduce((n, r) => n + (r.filesRead || 0), 0),
  findings: ok.reduce((n, r) => n + (r.findingsWritten || 0), 0),
  unreadNotes: ok.filter((r) => r.notes && r.notes.length).map((r) => `batch${r.batch}: ${r.notes}`),
  failedBatches: results.map((r, i) => (r ? null : i)).filter((x) => x !== null),
}
