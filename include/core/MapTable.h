#pragma once
#include <cstddef>

struct MapInfoEntry {
    const char* key;         // e.g. "MAP0_S00"
    const char* prefix;      // Chunk/map prefix e.g. "THR", "SC", "SU", "ER"
    const char* description; // Decomp-verified description from map_registry.c
};

// Decomp-verified map registry table (sourced directly from SlickAmogus_silent-hill-decomp map_registry.c & map_info.c)
static const MapInfoEntry MAP_REGISTRY_TABLE[] = {
    { "MAP0_S00", "THR", "Old Silent Hill - Intro" },
    { "MAP0_S01", "THR", "Old Silent Hill - Cafe" },
    { "MAP0_S02", "ER",  "Old Silent Hill - Bonus Areas" },
    { "MAP1_S00", "SC",  "School - 1F, Courtyard, Basement" },
    { "MAP1_S01", "SC",  "School - 2F" },
    { "MAP1_S02", "SU",  "School Otherworld - 1F, Courtyard" },
    { "MAP1_S03", "SU",  "School Otherworld - 2F, Roof" },
    { "MAP1_S04", "SU",  "Unused" },
    { "MAP1_S05", "SU",  "School - Boss (Split Head)" },
    { "MAP1_S06", "SC",  "School - 1F, Basement (Post-Boss)" },
    { "MAP2_S00", "THR", "Old Silent Hill - Streets" },
    { "MAP2_S01", "ER",  "Church" },
    { "MAP2_S02", "SPR", "Central Silent Hill - Streets" },
    { "MAP2_S03", "THR", "Unused" },
    { "MAP2_S04", "ER",  "Police Station" },
    { "MAP3_S00", "HP",  "Hospital - Pre-Kaufmann" },
    { "MAP3_S01", "HP",  "Hospital - 1F, Basement (Post-Kaufmann)" },
    { "MAP3_S02", "HU",  "Hospital - Antique Shop Cutscene" },
    { "MAP3_S03", "HU",  "Hospital Otherworld - 2F, 3F" },
    { "MAP3_S04", "HU",  "Hospital Otherworld - 1F" },
    { "MAP3_S05", "HU",  "Hospital Otherworld - Basement" },
    { "MAP3_S06", "HP",  "Hospital - 1F (Post-Otherworld)" },
    { "MAP4_S00", "SPR", "Unused" },
    { "MAP4_S01", "ER",  "Green Lion Antiques" },
    { "MAP4_S02", "SPU", "Central Silent Hill Otherworld - Streets" },
    { "MAP4_S03", "SPU", "Town Center - Mall, Boss" },
    { "MAP4_S04", "HU",  "Hospital - 1F (Lisa Cutscene)" },
    { "MAP4_S05", "SPU", "Central Silent Hill Otherworld - Boss" },
    { "MAP4_S06", "SPR", "Unused" },
    { "MAP5_S00", "DR",  "Sewers" },
    { "MAP5_S01", "RSR", "Resort Area - Streets" },
    { "MAP5_S02", "ER",  "Annie's Bar, Indian Runner" },
    { "MAP5_S03", "ER",  "Norman's Motel" },
    { "MAP6_S00", "RSU", "Resort Area Otherworld - Streets" },
    { "MAP6_S01", "ER",  "Lakeside Pier - Boat" },
    { "MAP6_S02", "RSU", "Lakeside Pier, Lighthouse" },
    { "MAP6_S03", "DRU", "Sewers (To Amusement Park)" },
    { "MAP6_S04", "APU", "Amusement Park" },
    { "MAP6_S05", "APU", "Unused" },
    { "MAP7_S00", "ER",  "Nowhere - 1F (Lisa Cutscene)" },
    { "MAP7_S01", "ER",  "Nowhere" },
    { "MAP7_S02", "ER",  "Nowhere - Cutscene (Alessa/Dahlia)" },
    { "MAP7_S03", "ER",  "Nowhere - Final Boss" }
};
static const size_t MAP_REGISTRY_TABLE_COUNT = sizeof(MAP_REGISTRY_TABLE) / sizeof(MAP_REGISTRY_TABLE[0]);
