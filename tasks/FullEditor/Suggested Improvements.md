# User-Suggested Improvement List

> Some of these may be hard to understand, or not align with the repo's current state. I quickly jotted these down while experimenting with the program, so take these with a grain of salt.

## AD-HOC
- [ ] DWAVE.TIM missing, causing graphical glitch in chunk manager (potentially from extraction and/or name, other causes, check if new extraction scripts from latest repo versions)
- [ ] Ensure no parent directory usage (`..\..`)
- [ ] Console should be selectable (but read-only, and without "Copy to Clipboard" button)
- [ ] Adding entirely new chunk files and having them appear in game (file table update, not just size) --> need to allocate to a panel, likely Chunks (should show confirmation prompt)
- [-] Adding new prefixes entirely for operational simplicity (custom prefix but test with same files) --> stubs added
- [x] Hide/show terminal (background, not 'Console' panel) should be an option
- [-] Hide/show certain viewports from settings menu --> stubs are implemented
- [x] Fix default camera position (not tied to a selected 'map')
- [x] Ensure prefurbished folder layout is automatically generated (`workspace`, `assets`, subdirectories) after selecting in configuration
- [ ] Undo buffer should cycle through each viewport (for those in main viewport loop)
- [ ] Ensure polymorphisms are fully utilised where necessary
- [ ] Clean source code names up to their current names ('ViewViewport' is awkward and relates to 'Scene')
- [ ] Test run in virtual machine (and another computer) with MINGW64 (fully done by user, no AI)
- [ ] Check for any issues that could involve little-endianness
- [ ] Reassert max file size (currently at 256.0KB, use binary overlay or otherwise to set max)
- [ ] Launch game button (PC Port), should edit config to load directly into map (Python search/replace) or chunk (macro/scripting)
- [ ] Button to compile original disc image (with CUE, see if options for multithreading and should be set by user)
- [ ] Simplify config file to `config.json` (to one file, currently `chunk_manager_config.json`)
- [ ] Extend `dictionary.json` for map name enums and custom prefix names, other aliases (global objects, textures, etc)
- [ ] Options for compiler type, added flags
- [ ] Linux and MacOS support
- [ ] Install guide (should automatically download libraries -- specific version not latest)
- [x] Deprecate unused Python scripts, identify which ones are used
- [ ] Find patches applied to PC Port source code, then write a Python patcher script if the original game code is problematic
- [ ] Ensure fsqueue2 and fsqueue3 patches are either applied or redundant for the PC Port
- [ ] Investigate role of CMP files
- [ ] Make workspace folder names consistent with panel names
- [ ] Add some RTF files for Usage, Documentation that are explorable within the editor

## GUI
- [-] Right-click menu (cut, copy, paste -- replaces, delete, resize) --> partially implemented with stubs
- [ ] Add multiselect for Objects and Faces
- [ ] Change multiselect (manual selection) binding to SHIFT (not Control, options menu may not be allocating this correctly, user check)
- [ ] Left click + drag to select multiple (click vs drag)
- [ ] Gizmos on all editable items (for multiple select, should just take centroid), should quantise/snap automatically --> move, resize/stretch, rotate
- [ ] Aliases should appear in legend (small text, in brackets)
- [ ] View all (prefix) chunks in legend
- [ ] Moving up/down speed option in settings, baseline speed should be higher
- [ ] Orthogonal tooltip when pressing ` should be Blender-like and track mouse movement upon release (have center area for no selection)
- [x] Add stubs for top bar functions (File >> Save Selection, Save All, Open Workspace, Save (As...) Workspace, Clear Workspace, Revert to Defaults, Quit; Edit >> Undo, Redo, Undo History, Preferences; Panels >> each panel as toggleable option, Background Console, Lock Panels; Viewport >> Sync Viewports (Enabled by default), Resolution Scale (1x, 2x, ...), Backface Culling, Axes >> (Show Axis, Show Axis Labels), Gridlines; Help >> Startup Guide, Usage, Naming Conventions, File Extensions, Documentation, GitHub, About) --> should be bound to Alt keys
- [ ] Startup Guide documentation (coloured text) on first launch (check with config variable)
- [ ] Keyboard shortcuts to deploy/extract
- [ ] Wireframe mesh width should be integer scale, not float
- [ ] Wireframe mesh width should affect both 'Local Geometry' and 'Collision' in the same way (now: Collision uses its own wireframe drawing but it needs to use the global `Wireframe.cpp`)

## CHUNKS
- [ ] Should ensure all active/viewport chunks only belong to one prefix

## MAPS
- [ ] Reorganise dropdown order (Maps -- limit height to bottom of lowest entry, then Actions)
- [ ] Sky colour should be the same as the chunk fog colour
- [ ] Should patch enums when adding in new maps / prefixes
- [ ] Remove const component to allow map prefixes/names to change, right click may allow to change, should use Dictionary
- [ ] Validator: Check if all chunks with waypoints/doors have chunk data
- [ ] Explore directly editing map files VS JSON/source code (preferred)

## SCENE
- [ ] Add checkboxes in 'Tools' menu for viewport (all by default should be off, all are read-only): boxes of enemy spawns, annotations of intended character path, double clicking on doors should take to linked chunk (and pop up message to indicate that a new map area has been reached, prompt to enter), lighting, 

## LOCAL GEOMETRY
- [ ] Create blank chunk with no geometry/data asides from floor (should notify of blank spaces)
- [ ] Ensure geometry only belongs in one chunk (doesn't have to be where it originally came from, requires validation and reindexing of both, doesn't have to be reversible), should have option to separate into their own meshes
- [ ] Connecting/disconnecting meshes by selection
- [ ] Validator: Check if all polygons are valid (GOOD triangles, quads?, BAD pentagons, ...)
- [ ] Validator: Check if duplication check (if any vertices in same spot, from same or different meshes)
- [ ] Validator: Check if all geometry is quantised correctly (might want to limit this to selected geometry only)

## GLOBAL GEOMETRY
- [ ] Set aliases to global geometry
- [ ] Show centroid of object (unsure if assumed to be center)
- [ ] Importing objects with mapping (OBJ importer, requires validation and centroid to be set)
- [ ] Should add 'View' and 'Edit' options in 'Tools' panel --> default to be 'View', 'Edit' should allow for faces/vertices to be edited, needs to carefully affect instantiated geometry upon save --> should borrow most of its edit logic from 'Local Geometry'
- [ ] Remove map prefix options in Global Geometry for missing options, should include other files
- [ ] Add non-map global geometry (objects)
- [ ] Hovering over textures should highlight on geometry
- [ ] Add save functionality (test forwards/backwards pipeline round-trip)
- [ ] Add free camera checkbox in 'Tools'

## COLLISION
- [ ] Global object collision missing in 'Collision' viewport
- [ ] Remove wall occlusions (move into 'Camera Paths', or hide by default)
- [ ] Make colour scale clearer (seems to cycle to red more than once)
- [ ] Add two adjustment modes of 20x20 grid: painter (set height in 'Tools', click and drag across area, rectangle with click/drag/SHIFT), mover (manually slide up/down grid, same as before with click and drag and optionally SHIFT for rectangular)
- [ ] Closed rooms --> box select to define space (connect boxes together at touching points)

## WAYPOINTS
- [ ] Destination map is always MAP0_S00 (when it obviously shouldn't be)
- [ ] Quantise positions automatically across 4096 precision
- [ ] Waypoints ('trigger param' etc) should be comprehensible
- [ ] Doors should be click and drag to connect
- [ ] Saving to source code inconvenient, keep workspace copies
- [ ] Filter event types to just doors/waypoints, not just all events (current)
- [ ] Allow for full reversion to originals (backed up)

## LIGHTING
- [ ] Add panel for Lighting (makes use of 3D viewport)
- [ ] Should simulate lighting conditions, fog and environment colours

## CAMERA PATHS
- [ ] Rename panel from 'Camera' to 'Camera Paths'
- [ ] Add wall occlusion logic from Collision (makes use of chunk + map data)

## IN-GAME MAPS
- [ ] Add panel for paper maps and annotations, link these to waypoints/points of interest (similar split panel as Global Geometry viewport)
- [ ] Allow for unavailability of map in a given area (if haven't accessed yet)

## SPAWNS
- [ ] Add 3D viewport component (similar to global objects)
- [ ] Drag in enemies into positions (XZ, height)
- [ ] Validate entire prefix/map to ensure there are no more than 4 enemies in an area

## AUDIO
- [ ] Add sound emissions (makes use of 3D viewport)
- [ ] Add background track to room/chunk areas

## EVENT SCRIPTS
- [ ] Add panel for event scripting
- [ ] Audio track syncing
- [ ] Provide (editable) dialogue (immediately from source file, option to write to binary directly) with timing
- [ ] Run through timing of audio/dialogue

## TEXTURE EDIT
- [ ] Adding custom textures to a specific directory (`data/workspace/textures/custom`), button to open directory, should be a mix of PNGs (with CLUTs) and TIMs
- [ ] Add import/export for TIM files (using TIM and PNG converters, labels with information such as number of CLUT rows, have maximum)
- [ ] Ensure checksum doesn't exclude custom files
- [ ] Add texture edit paint (draw, save, revert)
- [ ] Add CLUT row / palette editor (save, revert)
- [ ] Allow to add new CLUT rows (until there are 16 used)
- [ ] Investigate if additional limitations apply other than 16 CLUT rows occupied (especially in PC Port where the loaded TIM ceiling is expanded massively)
- [ ] Add independent undo/redo buffer
- [ ] Fix false transparency/translucency for objects that actually opaque in-game (issue with CLUT parsing from earlier code, requires user intervention/crosschecking)

## TEXTURE MAP
- [ ] Add CLUT row viewer (instead of just slider)
- [ ] Improve rotation buttons (add key bindings, add icons)
- [ ] Indication of which UV coordinates are which (out of the four, colours)
- [ ] Fix texture palette mapping
- [ ] Fix texture buffer (on select, cosmetics with border and excessive padding)
- [ ] Add reset to default button (for that particular face, careful with remapping with new vertices)
- [ ] Add zoom into coordinates
- [ ] Add a slider for view subdivision size
- [ ] Add texture individual UV point change (right click + drag)
- [ ] 128x256 textures should be half width (tiles same size as in 256x256 relative to panel width)
- [ ] Check to see if any other dimensions are written in (outside of 128x256, 258x256)
- [ ] Tile painting --> hold space (with some message at top of viewport, centered, saying '(Texture Paint)'), double click to toggle

## OUTLINER
- [ ] Expand/collapse all button (chunk information)
- [ ] Clean mesh information (max)
- [ ] Hyperlinks per chunk should take camera (position and angle) to center of chunk