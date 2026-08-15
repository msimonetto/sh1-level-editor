# Task: PLM Object Manager

This task tracks the development of the PLM Object Manager for the Unified C++ Editor. The goal is to allow safe browsing, editing, creation, and removal of global objects (`_GLB.PLM`), while explicitly enforcing that global objects cannot have their geometry edited from within a local chunk session viewport.

For the complete architectural design, phased breakdown, and current open questions, see the [Implementation Plan](IMPLEMENTATION_PLAN.md).

## Status

**Active** — Implementation plan defined and updated for the Unified C++ Editor. Awaiting user feedback on open questions before commencing Phase 1.

## Next Steps

*(Sourced from the Implementation Plan)*

- [ ] **Phase 1 (Window A)**: Enforce read-only infrastructure for global objects in the `Viewport3D` (blocking geometry/vertex edits).
- [ ] **Phase 2 (Window B)**: Implement the Dependency Index logic and `_GLB` loading in C++.
- [ ] **Phase 3 (Window B)**: Render global PLM objects in an ImGui preview viewport.
- [ ] **Phase 4 (Window B)**: Build the `_GLB.PLM` serializer in C++ and verify roundtrip.
- [ ] **Phase 5 (Window B)**: Implement Add/Remove CRUD operations and the Chunk Patcher to automatically fix `mesh_id` shifting across chunk files.
- [ ] **Phase 6 (Window B)**: Add PS1 VRAM/size validation and ImGui budget tracking UI.
