# Game Target Directory (`game/`)

This directory is the target workspace for linking the Silent Hill 1 Level Editor with playable game environments (such as the PC Port decompilation or future PSX disc image patching pipelines).

> **Note:** No copyrighted game assets or binaries are included with this repository. You must provide your own legally acquired game files.

---

## 1. PC Port Setup (`silent-hill-decomp`)

The level editor can deploy modified map chunks, textures, models, and overlay data directly into the PC decompilation's asset override directories.

### Setup Instructions:
1. Clone the PC Port repository into this directory (or inside `game/PC/`):
   ```bash
   git clone https://github.com/SlickAmogus/silent-hill-decomp.git game/PC
   ```
   Make sure to perform `git submodule update --init --recursive` for PsyCross.

2. Build the PC port according to its build instructions (https://github.com/SlickAmogus/silent-hill-decomp).
3. Edit the following in the PC Port's config (`game/PC/pc_port/build/config.cfg`):
   `allow_loose_files = 0` --> `allow_loose_files = 1`

4. Add a legally obtained copy of Silent Hill 1 disc image to: `game/PC/pc_port/build/gamedata/SLUS-00707.bin`
5. Run the editor. In Edit >> Settings, configure the editor's game directory config to `game/`

**Pipeline / Workspace Settings** (under `Settings` or `Config`) to point to your PC port root directory and its `override/` folder.

### Known Engine Considerations:
- When loading custom or expanded chunk data, certain decompilation builds may require adjusting the filesystem queue buffer sizes (`fsqueue2` / `fsqueue3`) in the decompilation source to prevent heap overflows on heavily modified chunks. I've confirmed this isn't a problem in the latest PC Port, but older commits may be affected.

---

## 2. PSX Disc Image / Patching (Future Support)

This directory also serves as the workspace target for:
- Extracting raw assets from PlayStation 1 `.BIN` / `.CUE` images (`SLUS_007.07`).
- Future direct binary image rebuilding and patching workflows.
- Will use directory `game/PSX/`
