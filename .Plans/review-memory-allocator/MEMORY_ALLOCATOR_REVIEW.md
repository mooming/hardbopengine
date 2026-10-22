# Judgement review — `fix/memory-allocator-defects`

Reviewed 2026-10-20. Findings only: no engine file was edited. Machine-readable pass:
`.Plans/review-memory-allocator/memory01.json` (27 findings, `verify-findings.py` exit 0).

## 1. Scope

| Item | Value |
| --- | --- |
| Branch at review time | `fix/memory-allocator-defects` |
| Confirmed target | `34a0410` (hb-docs guide) + `626249e` (allocator findings + two checkers) |
| Owner-confirmed scope | the 2 commits ahead of `master`, findings only, no code edits |
| Landed during the review | `9fa128f`, `12a8715` (test-macro unification) — outside scope, noted where they change the evidence |

The owner's opening snapshot showed `master` at `5bfab87`; the live checkout was already this
branch, so the review targets the memory work, not the Container ring queue.

## 2. Method and evidence

| Step | Command | Result |
| --- | --- | --- |
| Build + suite, 3 configs | `.pi/skills/hb-standards/scripts/runtest.sh {Dev,Debug,Release} 600` | 66 collections / 423 testlets pass in all three |
| Standards gate on the reviewed commit | `check.sh 626249e --no-build` | 1 grep failure, 3 backlog layers red, 13 files non-conformant |
| Comment ban, per file | `comments.py <each touched header>` | 3 `POINTER-PATH` violations |
| Feature-macro compilation | `-fsyntax-only` over 12 Memory TUs with `PROFILE_ENABLED=1`, `MEMORY_VERIFICATION_ENABLED=1` | **10 of 12 fail**; one identifier is the sole cause; with it corrected all 12 compile |
| Defect-class sweeps | `#ifdef PROFILE_ENABLED`, printf-style `Assert`, old spelling, `hbe::source_location`, `<= 0` | clean — the `<= 0` spellings are house style, see *Owner rulings* |
| Citation integrity | `verify-findings.py .Plans/review/memory01.json` | exit 0 — all 27 citations resolve |

Two caveats recorded so the evidence is not over-read:

* The three-configuration run executed on the tree as it moved to `12a8715`, not on the
  `626249e` blob. `626249e` was self-consistent (`build.sh -test` passed `-D__UNIT_TEST__` while
  the sources guarded on `__UNIT_TEST__`), so this is a statement about the branch tip, not about
  the reviewed commit in isolation.
* `PROFILE_ENABLED`, `MEMORY_VERIFICATION_ENABLED` and friends are `0` in `Engine/Config/BuildConfig.h`,
  so no configuration in `check.sh` compiles the code most of this commit touched. That is what made
  the syntax layer above the only real test of the commit's central claim.

## 3. What the commit gets right

Verified closed, each checked rather than assumed:

* Free-list terminator. The constructor writes `i + 1`, so the sentinel is `numberOfBlocks`;
  `Deallocate` now writes `numberOfBlocks`, `ReadNextIndex` accepts `<=` and `AllocateBlock` stops at
  `>= numberOfBlocks`. The old `numberOfBlocks - 1` handed the last block out twice.
* Wrong-allocator frees. `PoolAllocator`, `StackAllocator` and `MonotonicAllocator` now free their
  backing buffer to the recorded `parentID`; `MultiPoolAllocator` and `ThreadSafeMultiPoolAllocator`
  open a parent scope for the whole destructor.
* Logging under the allocator lock. `Deallocate` and `PrintUsage` now snapshot state, release `lock`,
  then log — the old shape logged inside a non-recursive `std::mutex` critical section.
* Move-ctor proxy repoint. `banks` grows, so `PoolAllocator` really is moved; before the fix
  `allocProxy.allocator` kept pointing at the moved-from object.
* `ReportDeallocation` fall-through. The `usage < allocated` branch used to reach `usage -= allocated`
  and wrap the counter; the added `return` closes it.
* `#ifdef PROFILE_ENABLED` → `#if PROFILE_ENABLED` in `NewBankAllocate`. `PROFILE_ENABLED` is always
  *defined*, so `#ifdef` was permanently true. No other site of that misuse remains in the tree.
* Null-proxy dispatch in `MemoryManager::Allocate/Deallocate`, `SetScopedAllocatorID` logging the same
  `id twice, the `GetAllocatorProxy` re-`placement new` of a live static, `MultiPoolAllocator::ReportConfiguration`
  the missing `else` in both `ReportConfiguration` bodies, whose absence dereferenced `configs.end()`, `requested <= 0`, the
  `hbe::source_location` family (gone tree-wide), `ReportMultiPoolConfigutation` (no stale caller),
  the anonymous-union `buffer`/`bufferPtr` pair, the printf-style `Assert` (zero left), `Log(Warning)`
  → `Log(Verbose)` on a successful config lookup (correct: it is a trace, not a warning).

The M1–M4 contention harness is a real addition: gated workers, a deadline on every wait, and a
`static_assert` on the free-order permutation. Its numbers are the most useful output of the branch.

## 4. Findings by severity

| # | Severity | Where | Finding |
| --- | --- | --- | --- |
| 1 | **high** | `InlinePoolAllocator.h:91` | `std::memset(buffer, …)` names a member that does not exist (it is `block`); 10/12 Memory TUs stop compiling with the feature macros on, and it masks every other profile/verification path |
| 2 | **high** | 3 headers | API-reference pointers installed to `docs/Memory/{PoolAllocator,MultiPoolAllocator,ThreadSafeMultiPoolAllocator}/index.html`, which do not exist; `comments.py` refuses them |
| 3 | **high** | `MemoryManager.h` `New`/`NewArray` | `Assert(ptr != nullptr)` then placement-new: `Assert` is a no-op in Release, so the null pointer is still used — the class of bug the commit fixed elsewhere |
| 4 | **high** | `PoolAllocator.cpp:50` | same shape: assert, then `&buffer[0]` and a free-list build through it |
| 5 | open question | `MemoryManager.cpp` `GetAllocatorProxy` | `FatalAssert` is declared outside the `!RELEASE_BUILD` block, so it aborts in every configuration today and a survivable invalid-id path became fatal in Release. The retained `static AllocatorProxy dummy{}; return dummy;` is **not** a defect — it is the house convention (42 of 75 `FatalAssert` sites are followed by a return) and it keeps the code correct if `FatalAssert` ever goes inert in Release. What is missing is only the record of which policy is intended. |
| 6 | **high** | `ThreadSafeMultiPoolAllocator.cpp` testlet | "Pool Buffer Returns To Its Recorded Parent" never observes the destination allocator, so it cannot fail for its own bug |
| 7 | **high** | `.pi/skills/hb-docs/scripts/quoted_blocks.py` | 625 new lines wired to no gate; run by hand it already finds 1 stale + 1 unresolved block |
| 8 | **high** | `JOURNAL.md` | no entry, no `.Plans/PLAN_*.md` for either reviewed commit (work-policy 2, 3, 10) |
| 9 | **high** | 104 files | `#endif //TEST_ENABLED` — clang-format refuses the spacing (introduced by `12a8715`, not by the reviewed pair) |
| 10 | medium | `PoolAllocator.cpp` `Allocate` | a failed allocation is reported to the profiler, inflating `stats.usage` — the source of the underflow the commit just guarded |
| 11 | medium | 2× `ReportConfiguration` | `Assert(reportedBlocks == peakBlocks)` is true by construction; it cannot fail and its message over-claims |
| 12 | medium | `ThreadSafeMultiPoolAllocator.cpp` | `BankGenerationLog` / `BankUsageRecord` are namespace-scope types in a `.cpp` without an anonymous namespace — external linkage, ODR hazard |
| 13 | medium | same file | a thread-local write plus five stores on every `Allocate` to enable one Verbose line |
| 14 | medium | same file | `GenerateBank` must be called holding `lock`; the invariant is asserted nowhere and no design document exists |
| 15 | medium | contention harness | `static_assert` guards the stride against `ContentionOpsPerThread`, but the modulus is `numThreads × ContentionOpsPerThread` |
| 16 | medium | same | `Allocate` result unchecked, then freed with a non-zero size → `Assert(size == 0)` aborts the whole test binary in a debug build |
| 17 | medium | same | an exception in a worker reaches `std::terminate` and takes the suite with it |
| 18 | medium | 3 testlets | "Destroy In Own Scope" written three times over |
| ruled | `MemoryManager.cpp:700`, `PoolAllocator.cpp:40`, `MultiPoolAllocator.cpp:137` | unsigned `<= 0` — **blanket house style**, forward defence in case a value is later revised to signed. See *Owner rulings* |
| 20 | medium | `MemoryManager.cpp` `DeregisterAllocator` | leak reporting went from a Warning in every build to an `Assert`, i.e. silent in Release; the old early return also became fall-through |
| 21 | medium | `MultiPoolAllocator.h` | an allocator that owns a registered id declares no deleted copy/move; the proxy bug returns the day a move ctor is added there |
| 22 | medium | `PoolAllocator.cpp` move-assign | no self-move guard: destroys `*this`, then constructs from the destroyed `rhs` |
| 23 | low | `AllocatorScope.h` | constructor template still unconstrained after the sibling overloads gained `explicit` |
| 24 | medium | measured, not code | the shared thread-safe pool does not scale — see §5 |

### Owner rulings — decided during the review, not defects

* **Unsigned `x <= 0` is the project's house style, tree-wide.** Written where a value might later be
  revised to a signed integer, so the negative case is already handled without a follow-up edit. Ruled
  blanket, so `MemoryManager.cpp:700`, `PoolAllocator.cpp:40` and `MultiPoolAllocator.cpp:137` are closed
  as decisions, not defects. The measured cost under this build is nil: `clang++ -std=gnu++23 -Wall
  -Werror` is silent on `std::size_t len = v.size(); if (len <= 0)`, and `-Wtautological-compare` and even
  `-Weverything` produce no diagnostic for that shape either (clang warns on `unsigned >= 0`, not `<= 0`).
  Three consequences recorded so the policy is owned knowingly: the guards are dead code *today*, so no
  test can reach their negative branch; GCC's `-Wtype-limits`, which `-Wall` enables, **does** warn on this
  shape, so a future GCC configuration turns roughly two dozen sites into `-Werror` failures (unmeasured
  here — Apple's `gcc` is clang, so no GCC build exists on this machine to test); and `626249e` changed one
  site to `requested == 0`, which under this ruling is the deviation from house style rather than the fix.
  Nothing enforces either spelling, because no checker greps for `<= 0`, so no waiver mechanism is needed.
  Engine sources carry no comments, which is why the reason lives here and not beside the line.
* **The return after a `FatalAssert` stays** (row 5): `FatalAssert`'s activity in Release is a policy, not
  a contract, and `[[noreturn]]` must not be added — see row 5.

The JSON holds 27 entries: 11 high, 12 medium and 1 low are open findings, and 3 are **ruled** — recorded
owner decisions, so 24 defects remain open. The severity column above judges *impact* while the JSON
carries *confidence*, so the two axes differ on a few rows — a citation with high confidence can still be a
low-impact cleanup.

## 5. The measurement the new tests produced

`build/gate/Dev-run.log`, 20 000 op-pairs per thread, direct `Allocate`/`Deallocate` calls:

| N threads | shared ns/op | malloc ns/op | sharded ns/op | efficiency | shared ÷ malloc |
| --- | --- | --- | --- | --- | --- |
| 1 | 23.9 | 20.3 | 10.4 | 1.000 | 1.16× |
| 2 | 37.2 | 16.6 | 5.2 | 0.321 | 2.07× |
| 4 | 72.5 | 26.1 | 2.9 | 0.082 | 2.75× |
| 8 | 126.5 | 14.8 | 1.5 | 0.024 | 8.02× |

M3: cross-thread free costs the same as same-thread free (1.033×), so the cost is contention on the
single `std::mutex`, not ownership transfer. M4: bank growth inside `Allocate` measures typical 42 ns,
**p99 24 875 ns, max 169 541 ns** — a 600× tail spike, consistent with allocating a bank, constructing
a `PoolAllocator` and `std::sort`ing every bank while holding the lock.

The harness reports all of this as `lfwarn`, so the gate stays green by design. The branch already
contains `wip/option-c-sharded`, and the M2 sharded column (2.2× → 77× faster) is the argument for it.

## 6. Recommended next steps, in order

| Step | Action | Proof |
| --- | --- | --- |
| 1 | `buffer` → `block` at `InlinePoolAllocator.h:91` | the syntax layer below goes green over 12 TUs |
| 2 | add a feature-macro syntax layer to `check.sh`/`gate.sh` (`-DPROFILE_ENABLED=1 -DMEMORY_VERIFICATION_ENABLED=1`, `-fsyntax-only`) | a configuration nobody compiles stops being a place defects live |
| 3 | author the three `docs/Memory/…` pages with `docs_page.py`, or drop the pointers | `comments.py` reports 0 violations; `docs_coverage.py check Memory` |
| 4 | turn the four null-pointer asserts into release-safe guards (rows 3, 4) | suite green. Owner ruling: `FatalAssert` is a policy, not a contract — the `return` after it stays, and `[[noreturn]]` must **not** be added, because a `[[noreturn]]` function whose caller's tail was deleted is undefined behaviour |
| 5 | make the buffer-destination testlet observe its own claim, then de-duplicate the three copies | a red run against the pre-fix parent, proven by reverting one line |
| 6 | fix the harness gaps (15, 16, 17) | run under the existing `build-tsan` tree |
| 7 | wire `quoted_blocks.py` into layer 5 | `check.sh --all --no-build` shows it running |
| 8 | sweep `#endif //TEST_ENABLED` spacing (`<= 0` is ruled house style — nothing to sweep) | `check.sh --all --no-build` back to 0 grep failures |
| 9 | journal entry + `.Plans/PLAN_*.md` for the allocator work | work-policy 2, 3, 10 |
| 10 | decide the sharding question with the M1/M2/M4 numbers in hand | owner decision, not a code step |

Feature-macro syntax check, run from the repo root (self-restoring; refuses to start if either
file is already dirty):

```bash
python3 - <<'PY'   # flip the two macros, -fsyntax-only every Memory TU, restore in finally
# BuildConfig.h: '#define PROFILE_ENABLED 0' -> '1', MEMORY_VERIFICATION_ENABLED likewise;
# then for each Engine/Memory/*.cpp entry in cmake-build-debug/compile_commands.json run its
# own command line with -c replaced by -fsyntax-only and -o <obj> removed.
PY
```

Worth promoting into `.pi/skills/hb-standards/scripts/` as a gate layer — it is the only check that
proves anything about the code most of this commit touched.
