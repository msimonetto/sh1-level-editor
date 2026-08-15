# Task: Dependencies Panel (Workspace File Manager)

## Context & Motivation
Currently, panels like `GlobalGeometry` are pulling double duty as 3D viewports and global object discoverers. As the editor scales to handle more complex operations (importing files, copying objects between PLMs, tracking texture usage), we need a centralized manager to safely handle these actions.

The **Dependencies Panel** will act as the central nervous system for workspace file management. It bridges the raw `assets/` directory and the active `workspace/`, enforcing strict safety rules, managing budget allocations, and giving the user context-aware previews of what they are importing or editing.

---

## 1. Core Panel Functionality

- **Asset Library & Import:** 
  - Browse the raw `assets/` folder and manually bring files into the `workspace/`.
- **Context-Aware Dual Viewer:** 
  - The right side of the panel swaps its rendering logic dynamically based on selection (e.g., a 3D orbital viewport for `.PLM` geometry files, and a flat 2D image viewer for `.TIM` textures).
- **Prefix-Based Grouping:** 
  - The engine structures libraries strictly by map prefix (`THR`, `SC`, etc.). The manager restricts searches and organization to the active prefix.
  - *Cross-Map Duplication:* The engine does not natively support cross-map dependencies. If an asset from one map is needed in another, it will be duplicated under the target map's prefix and given a new numerical allocation to correctly track against the new map's memory budget. There is no messy "global" bucket.
- **Safety Locks & Removal Overrides:** 
  - The existing Chunk Manager will auto-pull required dependencies. 
  - If a file is currently being used by anything in the workspace, the Dependencies panel will **forbid** its standard removal and explicitly state which files require it.
  - A "Force Remove" override button (with a heavy warning message) will be included for debug purposes.

---

## 2. Bidirectional Dependency Tracking

Instead of relying on a heavy background indexer at runtime, tracking will be entirely managed via JSON definitions:
- **Forward Tracking (`dependencies.json`):** Used to see exactly what textures or sub-objects a selected `.PLM` or `.IPD` chunk requires.
- **Reverse Tracking (`dependents.json`):** A new, inverted JSON mapping that lets you select an asset (like a `.TIM` texture or `.PLM` object) and see every single chunk or global object in the workspace (or assets fallback) that actively uses it.

---

## 3. Workflow Integrations (Other Panels)

The introduction of the Dependency Manager will heavily benefit and integrate with the other editor panels:

- **Texture Map / Texture Edit Panels:** 
  - The available selection dropdowns will strictly only allow *workspace* TIMs to be selected.
  - The available list is defined by the intersection of the current `workspace` AND the currently active `prefix` (exceptions for non-conforming filenames can be addressed later).
  - *Texture Edit* will query `dependents.json` to show the user exactly what chunks/objects are using the texture being edited.
- **Local Geometry Panel:** 
  - When "plopping" down available PLM objects into a chunk, a dialog button will notify the user if new dependencies (textures, global objects) are being added to the workspace.
- **Global Geometry Panel:** 
  - Will query `dependents.json` to show the user exactly what IPDs/chunks use the currently viewed PLM object.
  - Will be stripped back to just a pure 3D viewport for editing the active PLM, shedding its previous file discovery responsibilities to the Dependencies panel.

---

## 4. Potential Issues & Redundancies to Address

- **JSON Synchronization:** `dependencies.json` and `dependents.json` must be updated simultaneously (e.g., when a user adds an object, or the Chunk Manager auto-pulls a texture). A quick validation check at startup (or prefix switch) is needed to verify the JSON arrays match actual files on disk and purge ghost entries.
- **Centralized Logic:** The Chunk Manager and Dependencies panel must share the exact same logic for reading/writing to the JSON files to prevent file locks or out-of-sync states.
- **Budget Reallocation ID Generator:** When duplicating a cross-map asset, the manager needs a robust ID generator that parses the target prefix's PLM headers to safely inject the new asset without overwriting existing data.
