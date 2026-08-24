# Dependencies Panel (Workspace File Manager) - TODO

## Completed 
- `[x]` **Bidirectional Dependency Tracking (Logic)**
  - Created `core/DependencyManager` to manage JSON-backed dependency maps.
  - Implemented forward tracking (`dependencies.json`) and reverse tracking (`dependents.json`), natively segmented by prefix.
  - Stripped out heavy binary IPD parsing from `ChunkManager`, allowing `DependencyManager` to resolve dependencies cleanly.
- `[x]` **Dictionary / Alias Manager (Logic)**
  - Spun off chunk aliases and prefix name mappings from `ChunkManager` into a new polymorphically available `core/Dictionary` module.
- `[x]` **Dependencies Panel Boilerplate**
  - Created `panels/DependenciesPanel` header/source, wired up to `main.cpp`, and removed the obsolete dependencies UI from `panels/ChunksPanel`.

## Up Next (In Progress / Pending)

- `[ ]` **Asset Library & Import UI**
  - Allow browsing of the raw `assets/` folder and manually pulling files into the `workspace/`.
- `[ ]` **Context-Aware Dual Viewer**
  - Implement a dynamic preview window on the right side of the Dependencies Panel.
  - Add 3D orbital viewport for `.PLM` geometry files.
  - Add 2D flat image viewer for `.TIM` textures.
- `[ ]` **Prefix-Based Grouping UI & Logic**
  - Restrict UI viewing and searching strictly to the active map prefix.
  - Implement **Cross-Map Duplication**: copying a file from one map prefix to another, generating a new allocation against the target map's memory budget.
- `[ ]` **Safety Locks & Removal Overrides**
  - Integrate reverse tracking (`dependents.json`) to prevent users from removing files actively in use.
  - Clearly display which workspace chunks/objects require a selected dependency.
  - Implement a "Force Remove" override button with a warning.
- `[ ]` **Workflow Integrations**
  - Update `TextureMap` and `TextureEdit` panels to query the `DependencyManager` and restrict selections *only* to workspace TIMs belonging to the current active prefix.
