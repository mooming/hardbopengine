# Project Memory & Constraints

## TriangleExample Renderer Redesign
- **Architecture**: Implement a Data-Oriented Design (DOD) renderer.
- **Constraints**:
    - **Strictly Local**: All implementation must be contained within `Applications/TriangleExample/`.
    - **No Engine Changes**: Do not modify any files in `Engine/` (including `Engine/Renderer`).
    - **No IRenderer**: Do not use the `IRenderer` interface or the `RendererFactory`.
    - **Pipeline**: Follow the Extract $\rightarrow$ Prepare $\rightarrow$ Submit flow locally.
- **Goal**: Replace the current stub/stateful renderer with a stateless, data-driven pipeline that feeds into the `SoftwareRenderer` (which now uses `OS::IWindow::SetPixels`).
