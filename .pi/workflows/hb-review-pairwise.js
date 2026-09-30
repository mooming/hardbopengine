export const meta = {
  name: 'hb-review-pairwise',
  description: 'Per-pair hb-standards judgement review of one module, citations machine-verified, 3 concurrent',
  whenToUse: 'Owner asks for a per-file standards review of a module off the main session. Pass args = {module, width, batches} to review another module; with no args it reviews the embedded Engine/Core table.',
  phases: [
    { title: 'Review', detail: 'one agent per header/source pair, findings gated by verify-findings.py' },
    { title: 'Audit', detail: 'prove every assigned file was opened and every citation resolves' },
  ],
}

const CORE_BATCHES = [
  {
    "label": "pair:CPUBudget",
    "files": [
      "Engine/Core/CPUBudget.h",
      "Engine/Core/CPUBudget.cpp"
    ],
    "index": 0,
    "out": ".Plans/review/core/batch00.json",
    "lines": 370
  },
  {
    "label": "pair:CommandLineArguments",
    "files": [
      "Engine/Core/CommandLineArguments.h",
      "Engine/Core/CommandLineArguments.cpp"
    ],
    "index": 1,
    "out": ".Plans/review/core/batch01.json",
    "lines": 75
  },
  {
    "label": "pair:Component",
    "files": [
      "Engine/Core/Component.h",
      "Engine/Core/Component.cpp"
    ],
    "index": 2,
    "out": ".Plans/review/core/batch02.json",
    "lines": 72
  },
  {
    "label": "pair:ComponentSystem",
    "files": [
      "Engine/Core/ComponentSystem.h",
      "Engine/Core/ComponentSystem.cpp"
    ],
    "index": 3,
    "out": ".Plans/review/core/batch03.json",
    "lines": 338
  },
  {
    "label": "pair:Debug",
    "files": [
      "Engine/Core/Debug.h",
      "Engine/Core/Debug.cpp"
    ],
    "index": 4,
    "out": ".Plans/review/core/batch04.json",
    "lines": 127
  },
  {
    "label": "pair:MainThreadTaskQueue",
    "files": [
      "Engine/Core/MainThreadTaskQueue.h",
      "Engine/Core/MainThreadTaskQueue.cpp"
    ],
    "index": 5,
    "out": ".Plans/review/core/batch05.json",
    "lines": 158
  },
  {
    "label": "pair:ResultPacket",
    "files": [
      "Engine/Core/ResultPacket.h",
      "Engine/Core/ResultPacket.cpp"
    ],
    "index": 6,
    "out": ".Plans/review/core/batch06.json",
    "lines": 352
  },
  {
    "label": "pair:ScopedLock",
    "files": [
      "Engine/Core/ScopedLock.h",
      "Engine/Core/ScopedLock.cpp"
    ],
    "index": 7,
    "out": ".Plans/review/core/batch07.json",
    "lines": 99
  },
  {
    "label": "pair:StreamDrainPolicy",
    "files": [
      "Engine/Core/StreamDrainPolicy.h",
      "Engine/Core/StreamDrainPolicy.cpp"
    ],
    "index": 8,
    "out": ".Plans/review/core/batch08.json",
    "lines": 578
  },
  {
    "label": "pair:SystemStatistics",
    "files": [
      "Engine/Core/SystemStatistics.h",
      "Engine/Core/SystemStatistics.cpp"
    ],
    "index": 9,
    "out": ".Plans/review/core/batch09.json",
    "lines": 253
  },
  {
    "label": "pair:Task",
    "files": [
      "Engine/Core/Task.h",
      "Engine/Core/Task.cpp"
    ],
    "index": 10,
    "out": ".Plans/review/core/batch10.json",
    "lines": 273
  },
  {
    "label": "pair:TaskProvider",
    "files": [
      "Engine/Core/TaskProvider.h",
      "Engine/Core/TaskProvider.cpp"
    ],
    "index": 11,
    "out": ".Plans/review/core/batch11.json",
    "lines": 1200
  },
  {
    "label": "pair:TaskRegistry",
    "files": [
      "Engine/Core/TaskRegistry.h",
      "Engine/Core/TaskRegistry.cpp"
    ],
    "index": 12,
    "out": ".Plans/review/core/batch12.json",
    "lines": 1020
  },
  {
    "label": "pair:TaskStream",
    "files": [
      "Engine/Core/TaskStream.h",
      "Engine/Core/TaskStream.cpp"
    ],
    "index": 13,
    "out": ".Plans/review/core/batch13.json",
    "lines": 1285
  },
  {
    "label": "pair:TaskStreamAffinity",
    "files": [
      "Engine/Core/TaskStreamAffinity.h",
      "Engine/Core/TaskStreamAffinity.cpp"
    ],
    "index": 14,
    "out": ".Plans/review/core/batch14.json",
    "lines": 315
  },
  {
    "label": "pair:TaskSystem",
    "files": [
      "Engine/Core/TaskSystem.h",
      "Engine/Core/TaskSystem.cpp"
    ],
    "index": 15,
    "out": ".Plans/review/core/batch15.json",
    "lines": 4149
  },
  {
    "label": "pair:Time",
    "files": [
      "Engine/Core/Time.h",
      "Engine/Core/Time.cpp"
    ],
    "index": 16,
    "out": ".Plans/review/core/batch16.json",
    "lines": 284
  },
  {
    "label": "pair:WorkItem",
    "files": [
      "Engine/Core/WorkItem.h",
      "Engine/Core/WorkItem.cpp"
    ],
    "index": 17,
    "out": ".Plans/review/core/batch17.json",
    "lines": 167
  },
  {
    "label": "headers:types",
    "files": [
      "Engine/Core/CommonMacros.h",
      "Engine/Core/CommonUtil.h",
      "Engine/Core/Types.h",
      "Engine/Core/Constants.h",
      "Engine/Core/TaskID.h",
      "Engine/Core/TaskStreamIndex.h"
    ],
    "index": 18,
    "out": ".Plans/review/core/batch18.json",
    "lines": 201
  },
  {
    "label": "headers:policy",
    "files": [
      "Engine/Core/ComponentState.h",
      "Engine/Core/Exception.h",
      "Engine/Core/Runnable.h",
      "Engine/Core/ScopedTime.h"
    ],
    "index": 19,
    "out": ".Plans/review/core/batch19.json",
    "lines": 97
  }
]

const SUMMARY = {
  type: 'object',
  additionalProperties: false,
  required: ['batch', 'label', 'filesAssigned', 'filesRead', 'findingsWritten', 'path', 'notes'],
  properties: {
    batch: { type: 'integer' },
    label: { type: 'string' },
    filesAssigned: { type: 'integer' },
    filesRead: { type: 'integer', description: 'files actually opened in full' },
    findingsWritten: { type: 'integer' },
    path: { type: 'string', description: 'the JSON file written' },
    notes: { type: 'string', description: 'anything not read, or a statement that the 10-finding cap was hit; else empty' },
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

// args arrives as JSON text, not as an object, so a caller who passes
// { module, width, batches } gets a string here and args.batches is undefined.
// Reading a property off that undefined is what killed an earlier run, so the
// parse happens before any property access.
function asObject(value) {
  if (value && typeof value === 'object') return value
  if (typeof value === 'string') {
    try {
      const parsed = JSON.parse(value)
      return parsed && typeof parsed === 'object' ? parsed : {}
    } catch (err) {
      return {}
    }
  }
  return {}
}

const cfg = asObject(args)
// startIndex lets a partial run be resumed without re-reviewing batches that
// already have a findings file; the original index, label and out path are kept
// so the output files still line up with the full 20-batch table.
const allBatches = Array.isArray(cfg.batches) && cfg.batches.length > 0 ? cfg.batches : CORE_BATCHES
const from = cfg.startIndex > 0 ? cfg.startIndex : 0
const batches = allBatches.filter((b) => b.index >= from)
const width = cfg.width > 0 ? cfg.width : 3
const moduleName = cfg.module ? cfg.module : 'Engine/Core'
log('args arrived as ' + typeof args + (typeof args === 'string' ? ' (' + String(args).slice(0, 40) + ')' : '') + '; batch table ' + (Array.isArray(cfg.batches) ? 'from args' : 'embedded') + ', index >= ' + from + '. ' + batches.length + ' batches over ' + moduleName + ', ' + width + ' at a time (owner cap), citations gated')

phase('Review')
const results = []
for (let c = 0; c < batches.length; c += width) {
  const wave = batches.slice(c, c + width)
  const out = await parallel(wave.map((b) => () => {
    const fileLines = b.files.map((f) => '    ' + f).join(String.fromCharCode(10))
    const only = b.files.length === 1
      ? 'This is a header-only slice: it has no .cpp counterpart, so every definition it exposes is inline and every behavioural rule applies to it directly.'
      : 'Review the header and its implementation as one unit - log-before-failing is only visible in the .cpp, [[nodiscard]] is only checkable in the .h.'
    const prompt = 'You are auditing one slice of the HardBop Engine C++ codebase (repo root: /Users/anav/atelier/hardbopengine) against its coding standards.\n\n'
      + 'Your files, and ONLY these (' + b.files.length + ' files, ' + b.lines + ' lines total). Open EVERY one of them IN FULL with the read tool - not an excerpt, not a grep hit:\n'
      + fileLines + '\n\n' + only + '\n\n' + RULES + '\n\n' + SKIP + '\n\n'
      + 'Write your findings as JSON to ' + b.out + ' using the write tool, shaped exactly:\n'
      + '    {"batch": ' + b.index + ', "label": "' + b.label + '", "filesRead": <int>, "findings": [{"file":"<repo-relative path>","line":<int>,"rule":"short name","evidence":"<verbatim source text from that line>","why":"one sentence","fix":"the concrete edit","confidence":"high|medium|low"}]}\n\n'
      + 'ZERO findings is a correct and welcome answer for a clean slice - ' + b.files.length + ' small files may well have nothing wrong with them. In that case still write the file with "findings": [], because a missing file fails the gate and is indistinguishable from a crash.\n\n'
      + 'Hard rules, because a previous run of this task invented defects:\n'
      + '- A validator will check every citation: the file must exist, the line must be within it, and your "evidence" must appear verbatim on or beside that line. Anything that fails marks this batch FAILED and you will have to redo it.\n'
      + '- Therefore never report a line you have not read, and never write from memory of a filename. If you cannot quote it, drop it.\n'
      + '- Report at most 10 findings, highest confidence first. If you hit that cap, say so in "notes".\n'
      + '- Read-only otherwise: do not edit, create, or delete any engine source, do not run clang-format, builds, tests, or check.sh, and never push. The only file you may write is ' + b.out + '.\n'
      + '- Do not launch subagents or workflows.\n\n'
      + 'Your structured return is a summary, not the findings: report batch ' + b.index + ', label "' + b.label + '", how many files you were assigned, how many you actually opened in full, how many findings you wrote, the path, and in "notes" name any assigned file you did NOT open, or state that you opened all of them.'
    return agent(prompt, {
      label: b.label,
      phase: 'Review',
      schema: SUMMARY,
      effort: b.lines > 3000 ? 'high' : 'medium',
      gate: 'python3 .pi/skills/hb-standards/scripts/verify-findings.py ' + b.out,
    })
  }))
  results.push(...out)
  const failed = out.filter((x) => x === null).length
  if (failed) log('wave ' + (Math.floor(c / width) + 1) + ': ' + failed + ' batch(es) failed the citation gate - reported, not retried')
}

phase('Audit')
const ok = results.filter(Boolean)
const shortRead = ok.filter((r) => r.filesRead < r.filesAssigned)
const reportedNotes = ok.filter((r) => r.notes && r.notes.length > 0)
log(ok.length + '/' + batches.length + ' batches passed the gate; ' + (results.length - ok.length) + ' failed or died; ' + shortRead.length + ' reported a file it did not open')

return {
  module: moduleName,
  batchesTotal: batches.length,
  batchesPassed: ok.length,
  filesAssigned: ok.reduce((n, r) => n + (r.filesAssigned || 0), 0),
  filesRead: ok.reduce((n, r) => n + (r.filesRead || 0), 0),
  findings: ok.reduce((n, r) => n + (r.findingsWritten || 0), 0),
  shortRead: shortRead.map((r) => r.label + ': read ' + r.filesRead + '/' + r.filesAssigned + '; ' + r.notes),
  notes: reportedNotes.map((r) => r.label + ': ' + r.notes),
  failedBatches: results.map((r, i) => (r ? null : batches[i].label)).filter((x) => x !== null),
}
