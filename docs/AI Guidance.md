# AI Guidance

This file defines how AI assistants and developers should structure work and research in this project. Read it when starting or switching development tasks.

---

## Core Principles

Before doing anything else, internalize these rules:

1. **Evidence over assumptions.** No field meaning, offset, or format detail is accepted as fact without cross-referencing decompiled source code (`game/PC/`) or a reproducible binary test. See `docs/research/FACTS.md` and `docs/research/HYPOTHESES.md`.
2. **Byte-for-byte precision is non-negotiable.** Any converter, serializer, or patch writer that does not produce valid, bit-identical output must be treated as broken.
3. **Never open large files directly.** Files >100KB (especially those in `data/`) will destroy context. Use scripts to inspect specific fields or byte offsets only.
4. **Do not dump large outputs to the terminal.** Pipe outputs to `data/temporary/` files and inspect with targeted grep or scripts.
5. **No multi-line inline shell scripts.** Write scripts to `scripts/temporary/` or `scripts/research/`, then run them.
6. **Protect `data/assets/`.** It contains original disc extracts and is strictly read-only. Never write new files there; output to `data/workspace/`.
7. **No Git commit or staging operations by AI.** All Git staging, commits, and pushes are reserved for the user.

---

## Session Initialization

When the user says **"start a new session on X"** or **"continue with [task]"**, do this — and only this:

1. **Find the task folder.** If the user specifies a task, locate its folder in `tasks/` (defaulting to `tasks/FullEditor/` for core editor features). If the task name maps to a subdirectory in `tasks/`, use that directly.
2. **Read the active task file.** Read `tasks/[task]/TASK.md` (and any related `TODO.md` / implementation plans) to identify the active goal, current status, and next steps.
3. **Check the Architecture Index.** Consult `docs/ARCHITECTURE.md` to pinpoint relevant classes, headers, and methods before making code changes.
4. **Begin work.** Rely natively on the current conversation memory. Only open `docs/formats/` files if you need to reference specific binary structures.
5. **Keep responses succinct.** Focus directly on the active objective and debrief clearly without unnecessary conversational filler.

---

## Managing Tasks (`tasks/` directory)

The `tasks/` folder holds ongoing development objectives. This works reasonably well under multiple simultaneous branches. Each objective has its own subdirectory:

```
tasks/
  FullEditor/        ← Master task channel: overarching features & subsystem links
  Dependencies/      ← Workspace file manager & asset dependency graph
  GlobalObjects/     ← PLM object manager & global prop libraries
  Waypoints/         ← Room linkage, door triggers, and navigation points
  BinaryOverlays/    ← Map overlay architecture, camera/spawn/event structs
  Audio/             ← PS1 audio player & environmental soundscapes
  ContextMenus/      ← Viewport right-click context actions
```

**Rules:**
- One `TASK.md` per folder. Keep it short — a current objective, bullet-point status, and next steps.
- When switching between objectives, update the current `TASK.md` with its latest status before moving on.
- When a task is fully complete, move its folder to `tasks/_completed/` and append a line to `CHANGELOG.md`.
- `TODO.md` holds only the *immediate* next 1-3 steps for the active session. Longer-term items belong in the relevant `TASK.md`.

---

## What Requires Evidence

- Every binary field interpretation must be backed by at least one of: decompiled source code (`game/PC/`), a byte-exact round-trip test, or cross-validation across 3+ sample files.
- Offsets and sizes must be validated by `struct.calcsize` or stride arithmetic, not assumed.
- Unknown fields must remain as `unk_*` until proven — do not rename prematurely.

---

## Updating Documentation

At the **end** of a session, when a task is completed, or when a fact/hypothesis changes:

- **New confirmed fact?** → Append to `docs/research/FACTS.md`. Remove it from `docs/research/HYPOTHESES.md` if it was previously a hypothesis.
- **New hypothesis?** → Append to `docs/research/HYPOTHESES.md`.
- **Task completed?** → Move its folder to `tasks/_completed/` and append one line to `CHANGELOG.md` (format: `- **[YYYY-MM-DD]** Description`).
- **Task progressed?** → Update the relevant `tasks/[name]/TASK.md`.
- **Immediate next steps?** → Write them to `TODO.md` (max 3–5 bullets).

Do **not** modify `docs/research/FACTS.md` with unverified claims. Do **not** move items from `HYPOTHESES.md` to `FACTS.md` without empirical proof.

---

## Experiment Workflow

Follow this loop for each unknown:

```
Hypothesis → Experiment → Evidence → Conclusion → Docs/Code
```

- Make one small, targeted change or test at a time.
- If the test confirms something: add to `docs/research/FACTS.md`, update code.
- If the test is inconclusive: refine the hypothesis in `docs/research/HYPOTHESES.md`.
- If the test disproves it: remove or correct the hypothesis.

---

## Reference: File Purposes at a Glance

| File / Directory | Purpose |
|---|---|
| `README.md` | Project overview and directory map — for humans new to the project |
| `CONTRIBUTING.md` | Guiding principles, research methodology, contribution guidelines |
| `TODO.md` | Immediate next steps (max 3–5 items, current session only) |
| `CHANGELOG.md` | Append-only log of completed work |
| `docs/ARCHITECTURE.md` | Authoritative C++ component map, class interfaces, and subsystem index |
| `docs/formats/` | Binary and intermediate format specs (IPD, Collision, Overlays, JSON) |
| `docs/research/FACTS.md` | Confirmed knowledge (decompilation/test citation required) |
| `docs/research/HYPOTHESES.md` | Open questions and provisional interpretations |
| `tasks/` | Per-objective folders with status and notes (`FullEditor/TASK.md` as master) |
| `tasks/_completed/` | Archived completed tasks |