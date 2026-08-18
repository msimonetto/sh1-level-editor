# Contributing

Contributions are more than welcome. The editor is in a solid position to be furnished in its capability of editing chunk data and binary overlays. Currently, the repository name is a slight misnomer since most capability is in geometry editing rather than full level design, but I aim to shift attention over to map/level-specific elements in the forseeable future.

As of August 2026, there are many unimplemented features: editing collision geometry, enemy placement, audio emission/trigger points, and other map-specific parameters, and so forth. Research needs to be updated further. Most specific modules/elements that are being worked on have their own task in `tasks/`, which I will widen in the near future. The history and the current state of development is outlined on [README.md](README.md), and partially on the [changelog](CHANGELOG.md). The bottom line is that component-specific progress is documented in greater detail in the `tasks/` folders, and many of these can be developed individually.

Additionally, this repository may seem to have appeared out thin air, but I have a (now deprecated) repository `sh1-level-editor-research` that's been kept private due to its presence of game extracts and data-containing JSONs. That had about 150 commits. I had also done research continuously for about 1-2 months before that on file formats before I produced successful conversion tools.

I'm open to discuss any concerns or suggestions, and I'm happy to debrief in more detail. You can contact me via the email given on my profile. I'm busy with university at the moment but I will try my best to respond.

---

## Overview of development principles

### Conversion tools
- Development should be mostly research-based, pulling evidence from documentation kept in this repository or externally from the decompilation projects.
- In working with conversion scripts, you should aim to develop both forwards and backwards conversion scripts, and as a bottom line should produce byte-to-byte exact files to their original form. Making use of Hex editors is highly recommended, especially when diagnosing problems with struct/header recognition. development.
- In terms of backwards conversion, JSON files are recommended as an intermediate step, and ensure raw binary sections are minimised/removed when converting to-from that intermediate form. Naturally, that wouldn't apply to media-related raw data.

### Documenting research
- Empirical discoveries backed by decompiled code or byte-exact tests must be added with citations to [`docs/research/FACTS.md`](docs/research/FACTS.md). Unproven theories or speculative fields belong in [`docs/research/HYPOTHESES.md`](docs/research/HYPOTHESES.md) until verified. These files are outdated and will be updated precisely.
- Task-specific research should be integrated into `tasks/`, and if meaningful should be brought into `docs/research/` when the task is fulfilled.

### Adhering to constraints of game engine
PS1-specific requirements are easy to overlook and haven't been fully documented. This was a critical factor when deciding to develop an independent level editing program in C++, as scripting everything manually in Blender Python API was buggy and prone to crashes, or even in Unity given the need for custom bitfields and floating-point arithmetic here.

- 3D editing operates in floating-point world coordinates (1 game unit = 256 raw units), but all geometry must cleanly quantize to signed 16-bit integers (`int16_t`) on disk for Q19.12 coordinates.
- Max 255 vertices per submesh, 16-unit max height, 40-unit chunk cell boundaries, enforced by `ChunkValidator`. There are also height constraints above/below.
- See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full C++ component map and class index before making structural changes. Update it if adding or removing callable classes or panels.

### Level editor
- Panels and viewport modes should be modular to existing structures. Extensibility is critical to this editor. Especially in the context of AI GUI development, it might require taking a step back to inspect what can be abstracted, as AI tends to work towards the path of least resistance for the specific feature you're implementing. It's also nice to standardise features and the validation measures across viewports.
- After successfully writing conversion scripts that maintain round-trip accuracy, you should aim to translate that into C++ code for chunk and/or binary data to be held in memory. Manually running Python scripts and reading/writing JSONs will massively reduce performance per face/vertex operation.
- Unknown and unparsed blocks should be held and preserved in memory, and dynamically move where necessary to prevent corruption.
- Constraints should be interactively maintained or checked (with indication) in the editor.
- A file table patcher should be employed to ensure game file positions (in minutes/hours) expand dynamically across the entire disc image.
- Proprietary structures should be visualisable before they are editable. An example of this is through chunk-specific collision and boundary data, containing 2.5D collision subcells or camera occlusion angles that can often be more complex than actual geometry data. This took some effort to produce visualisation that appeared within reason.
- Separating folders into the original clean assets (`data/assets/`), working project files (`data/workspace`), and resultant game deployment overrides (`game/PC/...`) or the disc image location, is critical. Temporary folders for experimentation or diagnostics should be kept separately.

---

## Game assets

> [!CAUTION]
> **Do not commit copyrighted game assets.**  
> Never submit pull requests or commits containing original retail game files, PlayStation disc ISOs, raw `.IPD`/`.PLM`/`.TIM`/`.BIN` extracts from commercial discs, or copyrighted game audio/textures.

Contributors must provide their own legally acquired copy of *Silent Hill* for development and testing. All extracted game data must remain in local workspace directories (`data/workspace/`, `data/assets/`, `data/temporary/`, `game/`), which are ignored by `.gitignore`.

---

## Immediate steps from the current state

### Research
- Researching unknown/unparsed file formats such as `.TMD`, `.CMP` (potentially), `.KDT`.
- Researching unknown components of binary overlays.
- Updating documentation to reflect recent developments in file formats and binary overlay analysis from decompilation project.

### Python scripting
- Migrating remaining Python script logic into C++, namely those surrounding file management or reading/writing dependency JSONs.
- Allow for user-made scripts with automatically furnished interface.

### Chunk-specific features
- Integrating a user-editable dependency tracker, which is currently a stub.
- Adding a texture editing viewport to directly update `.TIM` files, allow for import/export and custom files.
- Adding editing to collision data/viewport, should ideally make use of orthogonal 2D overhead view for collision walls.
- Gizmos for moving/rotating that adhere to game constraints.
- Editing geometry/UV mapping in global object files (`.PLM`).

### Map-specific features
- Editing binary overlay data directly for original PSX extracts, instead of exclusively through the C source code. May require some Python parsing at first.
- Visualising enemy placement.

### Miscellaneous
- For playtesting purposes, allowing the launch of the PC Port from the 
- A list of 100+ unimplemented minor features/fixes are specified in [Suggested Improvements.md](tasks/FullEditor/Suggested%20Improvements.md) from user testing.

---

## Building and testing

```bash
cmake -B build
cmake --build build
```

1. Launch `build/SilentHillLevelEditor.exe` and verify viewport navigation, chunk loading, and tool interaction without crashes.
2. If modifying Python tools, verify both directions. Ensure that these are placed in excluded directories such as `data/temporary/` of the project:
   ```bash
   python scripts/convert.py ipd-to-json <sample.ipd> -o <output.json>
   python scripts/convert.py json-to-ipd <output.json> -o <rebuilt.ipd>
   ```
3. Confirm no copyrighted game files, build artifacts, or temporary logs are staged.

---

## Discussion

For feature suggestions, bug reports, architecture questions, or reverse-engineering discussions, open a GitHub Issue or Discussion thread. For format corrections, include evidence (relevant hex offsets), sample file context, or decompiled code references.