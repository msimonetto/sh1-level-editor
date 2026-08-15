# Audio Player

**Goal:** Create an interactive audio player capable of simulating the game's audio environment based on selectable cues and modes. This tool will allow users to audition BGM tracks, ambient soundscapes, and sound effects exactly as the game engine would process them.

**Dependency/Relevancy:** 
- **`binary_overlay_analysis`**: This task heavily relies on the findings from the binary overlay analysis. Specifically, the audio player will use the `bgmIdx` (background music index) and `ambientAudioIdx` (ambient VAB index) extracted from map overlays (`s_MapOverlayHdr`). These indices map directly to the `g_BgmTaskLoadCmds` and `g_AmbientVabTaskLoadCmds` tables, determining which `.VAB`, `.KDT`, and `.SEQ` files are loaded.

**Requirements:**
- Developing this player requires an all-encompassing understanding of the decompiled PS1 audio code, specifically how `Bgm_TrackUpdate`, `Sd_AmbientSfxSet`, and Konami's `libsd` abstraction handle audio streaming and playback.
- Must support mapping overlay indices directly to the underlying raw audio files (`SND/MAP###.VAB` and `.SEQ` tracks).

**Planned Integration:**
- The audio player is planned to be added as its own dedicated tab within the `unified_cpp_editor` application. 
- *Note: Do not jump into implementation yet. This task is currently in the planning and research phase.*

**Next Steps:**
- [ ] Review the `libsd` and audio playback code in the decompiled source (`external/SlickAmogus_silent-hill-decomp/src/bodyprog/sound/`).
- [ ] Draft an architectural design for parsing and playing `.VAB` (samples) and `.SEQ` (MIDI-like sequences) natively in C++ for the `unified_cpp_editor`.
