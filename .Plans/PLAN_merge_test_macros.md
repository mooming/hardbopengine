# PLAN — Merge `__UNIT_TEST__` into `__TEST__`

**Date:** 2026-10-07
**Superseded 2026-10-08:** the surviving macro was renamed again, to `TEST_ENABLED`, because a leading double
underscore is reserved to the implementation and `docs/CodingStandards.md` forbids it. See the `JOURNAL.md` entry for
2026-10-08 21:42, which records that rename and its measurements. Everything below describes what was built at the
time and stays accurate as a record of it.
**Status:** executed 2026-10-08. Commits `5efa5de`, `621426f`, `9c0c6ca`, `74feb71`, plus the
documentation commit.

**Decision history.** I first recommended `TEST_BUILD`, on the grounds that commit `61c6152` had
already renamed `__DEBUG__` to `DEBUG_BUILD` / `DEV_BUILD` / `RELEASE_BUILD`, and that both
existing names begin with a double underscore, which `[lex.name]` reserves to the implementation.
That recommendation was accepted and then withdrawn: the instruction was to use `__TEST__` and do
only that. This plan was rewritten to match what was built. The reserved-identifier point stands
unaddressed by choice, not oversight.

## Goal

`__TEST__` and `__UNIT_TEST__` were defined together in every configuration that defined either,
and `__TEST__` gated **nothing**: zero occurrences in any `.h`, `.cpp` or `.inl` under `Engine`,
`Applications`, `Examples` or `External`. The pair therefore carried no meaning the single
surviving macro does not carry. Collapse them to `__TEST__` and delete `__UNIT_TEST__`.

Behaviour must not change, and it did not. The measured proof is that the flag count moved name to
name without any configuration's number changing.

## Verified facts the work rested on

| Fact | Evidence |
| ---- | -------- |
| `__TEST__` was dead in C++ | 0 hits in sources; it existed only in build configs and prose |
| No directive mentioned both macros | `grep '^#if.*__TEST__' \| grep __UNIT_TEST__` returned nothing |
| Nothing `#define`/`#undef`/`#error`ed either macro | searched all sources |
| 127 guarded regions, all trailing but one | 126 end the file; the exception is `TestMain.cpp`, the case `docs/CodingStandards.md` waives |
| Regions hold only test classes | 110 are `#include "Test/TestCollection.h"` + a `*Test : public TestCollection` declaration, 17 the matching definition |
| No production class changes layout behind the macro | same classification — a mismatched define yields a link error, never corruption |
| The 19 generated `CMakeLists.txt` come from `.project.config` | `Tools/MakeBuild/Application/MakeBuild/CMakeLists.cpp:135` |
| The generator hardcodes no macro name | `grep __TEST__ Tools/MakeBuild` returned nothing, so the submodule did not move |
| A duplicated `-D` is harmless | `c++ -Wall -Werror -D__TEST__ -D__TEST__` compiled clean on Apple clang 21 |
| `__UNIT_TEST__` does not contain `__TEST__` as a substring | `…T_TEST__` versus `__TEST__` — which is why a blind rename cannot merge them on its own |

## Where the definitions actually live

| Layer | File | Disposition |
| ---- | ---- | ----------- |
| Tree-wide per-configuration | `.project.config` `precompileDefinitionsDebug` / `…Dev` | the real source of truth |
| One module, all configurations | `Engine/Test/.module.config` plain `precompileDefinitions` | renamed, deliberately **not** re-scoped |
| Hand-written | `Engine/Renderer/Vulkan/CMakeLists.txt` | the only CMake file edited by hand; Renderer's `ignoreSubdirectories` keeps the generator away |
| Generated | 19 `CMakeLists.txt` | regenerated with `./generate_cmake_files.sh`, never hand-edited |
| Command line | `build.sh`, `build.bat` | `-test` is still the only route that puts the macro into Release |

## Steps, as executed

| # | Step | Result |
| - | ---- | ------ |
| 0 | Baseline before any edit | 392 testlets, 60 collections, 0 failures in Debug and Dev; flags `__TEST__` 451/451/0, `__UNIT_TEST__` 455/455/4 |
| 1 | `.project.config`, `Engine/Test/.module.config`, Vulkan line 29, `build.sh`, `build.bat` — pair replaced, not one half renamed | committed `5efa5de` |
| 2 | `./generate_cmake_files.sh` | committed `621426f`; diff is 39 lines, every one a definition line |
| 3 | 127 C++ sites: 127 `#ifdef`, 125 `#endif` labels, 2 message strings | committed `9c0c6ca`; 254 lines changed, none added or removed |
| 4 | 54 tooling literals in 7 scripts and `SKILL.md`, plus 3 judgement edits, plus `__pycache__` removed | committed `74feb71` |
| 5 | 107 `docs/` occurrences, `AGENTS.md`, `README.md`, the false-claim corrections, this file, `JOURNAL.md` | this commit |

## Gates and their results

| Gate | Result |
| ---- | ------ |
| `__UNIT_TEST__` residue in sources, build config, tooling | 0 hits |
| Regeneration fidelity | 39 changed lines, all definition lines; no target, source, include or link statement moved |
| Debug + Dev + Release build | exit 0, no `-Wmacro-redefined` |
| Flag count moved name to name | `__TEST__` 455 / 455 / 4 after, against `__UNIT_TEST__` 455 / 455 / 4 before |
| Testlet parity | 392 testlets, 60 collections, 0 failures in Debug and Dev — identical to baseline |
| Release guard preserved | built without `-test`, prints "built WITHOUT `__TEST__`", exit 1 |
| No new blank-line or include finding | finding set over all 127 files byte-identical before and after, reproduced against the original file and the original checker |

## Label spacing was preserved, not normalised

104 sites read `#endif //__TEST__` and 21 read `#endif // __TEST__`. Each kept its own spacing, so
the diff carries the identifier and nothing else. Normalising the 21 would have added unrelated
churn to a commit whose whole claim is that it changes one name.

## Left undone, on purpose

| Item | Why |
| ---- | --- |
| A4 at `Engine/Test/UnitTestCollection.cpp:119`, A5 at `:226` | Pre-existing: they reproduce at those line numbers against the untouched file and the untouched checker. `:226` is the A14-versus-A5 tension over a trailing region's closing brace that `docs/CodingStandards.md` already records for `TestMain.cpp`. A rule-table decision, not a side effect of a rename. |
| The 167 missing method pages and other sweep backlog | Pre-existing documentation debt, untouched. |
| `PROFILE_ENABLED 1` in `Engine/Config/BuildConfig.h` | The user's own uncommitted edit. Kept out of every commit. |
| `#ifdef` to `#if` conversion | Not asked for. |
| Reserved-identifier cleanup on `__TEST__` | Withdrawn by instruction. |

## Defect found beside the task, not caused by it

With `PROFILE_ENABLED 1` the suite aborts with `Trace/BPT trap` at `PoolAllocatorTest` TC2 after
26 passes and 3 failures, in both Debug and Dev. At `0` the same tree runs all 392 testlets clean.
Every parity number above was taken at `0` and stays comparable only while that flag is there, so
this plan's verification does not transfer to a profiling tree.
