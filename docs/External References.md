# References

## Primary Sources

- **SlickAmogus/silent-hill-decomp (GitHub)** – PC port of the Silent Hill 1 decompilation. Contains the fully decompiled game code, per-map overlay C sources, and the build system that compiles map overlays into DLLs. Primary reference for struct definitions, function names, and engine behavior. The `pc_port/docs/` directory contains extensive technical analysis (collision, trigger zones, modding guides, texture systems).
- **Vatuu/silent-hill-decomp (GitHub)** – Original Silent Hill PS1 decompilation (engine code only). The SlickAmogus PC port is a fork of this project. Useful for function lookups and understanding the original PS1 memory layout.
- **belek666/sh_ipd2obj (GitHub)** – IPD-to-OBJ converter written in C. Used as the initial reference to verify geometry parsing, UV mapping, and PLM struct layouts. Some of our early struct knowledge was derived from matching this tool's output.
- **Official PS1 Specs** – PlayStation endianness, GTE mathematics, and data type conventions.

## Secondary Sources

- **Sparagas/Silent-Hill (GitHub)** – "Silent Hill Hub," a community repository of file format documentation and modding tools. Provides general context on reverse engineering practices for the series. Note: Developed separately to the scripts included in this repo. *Note: Developed independently of this level editor project.*

## Key Citations

- File type definitions (from decomp): `.IPD (6) – Map/world geometry chunks`.
- Map overlay header: `STATIC_ASSERT_SIZEOF(s_MapOverlayHdr, 4172)` in `include/bodyprog/map/map.h`.
- Collision trigger struct: `STATIC_ASSERT_SIZEOF(s_CollisionTrigger, 4)` in `trigger.h`.
- Camera path struct: `STATIC_ASSERT_SIZEOF(VC_ROAD_DATA, 24)` — capacity `CAMERA_PATH_COUNT_MAX = 100`.
- Spawn info struct: `STATIC_ASSERT_SIZEOF(s_SpawnInfo, 12)` — `charaSpawnInfos[2][16]`.