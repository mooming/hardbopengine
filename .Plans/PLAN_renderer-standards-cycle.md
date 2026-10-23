# PLAN — Renderer module standards cycle

Third module on `.Plans/WORKLIST_hb-docs-standards.md`.

## Findings (measured 2026-10-23)

### Layout — 44 findings, 1 file

`Vulkan/VulkanRenderer.h` — private helpers declared above the public API (the
BoundedPriorityQueue shape), plus data after functions. Read and reorder per the
twelve-block table; data moves as a unit; `--init-order` gate after.

### Format

No clang-format failures recorded; re-measure touched files. A/B debt per touched file.

### Docs — 19 pages owed

- `VulkanCapabilities` (utility header) — class-level page.
- `VulkanRenderer` — 18 method pages (createinstance, createsurface, pickdevice, createdevice,
  querycapabilities, createswapchain, createrenderpass, createdepthresource, createframebuffers,
  createcommandbuffers, createvertexbuffers, releasevertexbuffers, createpipeline, createsyncobjects,
  createmetalsurface, createbuffer, recordframe, destroy).

Authored by one sequential subagent (owner's chosen model), verified by me with
`docs_pass.py signatures`, `docs_methods.py`, `htmlcheck`, `quoted_blocks.py`.

### Comments — 239 in 9 files

| File | Count |
| ---- | ----- |
| RenderCapabilities.h | 106 |
| VulkanRenderer.cpp | 46 |
| VulkanCapabilities.cpp | 23 |
| VulkanRenderer.h | 22 |
| RHICapabilities.cpp | 11 |
| RHICapabilities.h | 8 |
| RenderCapabilities.cpp | 6 |
| Vertex.h | 1 |
| VulkanRenderer.mm | — |

Stripped only after the pages own the prose; the .cpp implementation notes move by hand to a
design document (the RenderCapabilities.cpp header block documents the zero-descriptor rationale
— check the RenderCapabilities page owns it first).

## Order

1. layout → 2. format/seams → 3. docs (subagent) → 4. strip → 5. gate, ledger, journal
