# PLAN — Establish Allman as the declared brace style

**Created:** 2026-09-06 14:27
**Trigger:** `Applications/VulkanExample/Main.cpp` reported as not matching
`Engine/CodingStandards.{h,cpp}`. Scope was redirected by the user to the
standard itself: identify the correct base brace style, then revise
`Engine/CodingStandards.{h,cpp}` and `.clang-format`.

---

## 1. Goal

Determine, from evidence rather than assumption, which clang-format base
style and brace family the codebase actually follows, and make the three
authoritative artifacts state that style correctly and consistently.

**Out of scope (explicitly):** reformatting the 218 engine files, and any
change to `Main.cpp` (pending separate approval).

## 2. Method

Built a probe file containing every brace-bearing construct
(namespace / class / struct / enum / union / function / ctor-initializer /
if / else / for / while / do-while / switch-case / empty function /
empty record / lambda) and ran it through every `BreakBeforeBraces` value
clang-format 22 accepts. Then measured each `BasedOnStyle` candidate's
residual deviation against `Engine/CodingStandards.{h,cpp}` with all other
options pinned.

## 3. Findings

| Finding | Evidence |
|---|---|
| Brace family is **Allman** | `Allman` deviates from the exemplars in 1 construct; `Linux`/BSD 28, `Stroustrup` 35, `WebKit` 36, `GNU` 41, `Attach`/K&R 45 |
| **BSD/KNF is refuted** | KNF attaches `if (x) {` and `namespace x {`, breaking only for functions — the exemplars break for both |
| `BasedOnStyle: LLVM` is optimal | 0 residual deviation; `Microsoft` ties; `Chromium` 12, `Google` 16, `WebKit` 22, `Mozilla` 110, `GNU` 291; `ALS`/`Figure` rejected |
| **`BreakBeforeBraces: Allman` is not usable** | `BraceWrapping` is ignored under a named style — proven by byte-identical output for `SplitEmptyRecord: true` vs `false`. `Custom` is mandatory |
| Empty-body exemption is correct | attached `) {}` appears 144× repo-wide; split `{`⏎`}` 0× |
| The exemplars, not the config, were the outlier | `CodingStandards.{h,cpp}` were the only 2 of 251 files indented with 4 spaces, contradicting their own header comment |

## 4. Open decision deferred to the user

`NamespaceIndentation` is genuinely contradicted repo-wide and is *not* a
brace-style question:

| Value | Dirty files | Changed lines |
|---|---:|---:|
| `None` — current, and matching the rule text + both exemplars + `Main.cpp` | 218 | 32 163 |
| `All` — what the engine body is actually written as | 219 | 18 271 |

Kept at `None`. Flagged, not decided.

## 5. Steps and verification

| # | Step | Verification | Result |
|---|---|---|---|
| 1 | Resolve brace/base style empirically | probe matrix + per-base residual diff | ✅ done |
| 2 | Revise `.clang-format` | `clang-format --dump-config` new ≡ old | ✅ byte-identical |
| 3 | Correct the false "K&R variant" rule text in `CodingStandards.h` | review | ✅ done |
| 4 | Re-indent both exemplars to tabs + canonicalize | `clang-format --dry-run -Werror` | ✅ clean |
| 5 | Prove the reformat is non-semantic | whitespace-stripped token diff | ✅ only 6 `{}` merges + 2 line joins |
| 6 | Prove the exemplars still compile | `clang++ -std=c++23 -fsyntax-only -Wall -Wextra` | ✅ exit 0 |
| 7 | Commit the 3 files only | `git status` | ✅ done |

## 6. Deliberately not modernized

`AlignTrailingComments`, `AlignConsecutive*`, `SpacesInParentheses`,
`SpacesInAngles`, `SortIncludes` and `AfterControlStatement` were left in
their long-standing scalar spellings. In clang-format 19–22 several of these
became mappings/enums; writing the modern form makes older clang-format —
including CLion's bundled formatter — fail to load the file outright.
Behaviour is identical either way, as the resolved-config check confirms.

## 7. Follow-ups awaiting approval

1. `docs/CodingStandards.md` still states *"K&R variant — opening brace on the
   same line"*, contradicting the exemplars, `.clang-format` and this plan.
2. `Main.cpp` fixes: include-block inversion, 5 same-line braces, missing blank
   line before `return mesh;`, `printf` without `<cstdio>`, magic numbers,
   `SetIdentity` duplicating `Renderer::IdentityMatrix`, C-style cast.
3. `Engine/CodingStandards.{h,cpp}` belong to no CMakeLists target, so they
   compile into nothing — which is why they drifted unnoticed. Adding them to a
   build (or a format-check CI step) is what prevents a repeat.
