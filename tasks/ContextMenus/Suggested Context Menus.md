Here are my proposed context menus (not including the text inside of the brackets, separators are given). They can all be stubs for now, and there is some even more local polymorphism seeing it as I type everything out. Many of the multi-select faces should be batch operations and may narrow down the available options:

1. Viewports:
1a: Selecting local mesh(es) (in Scene or Local Geometry's Object mode): Cut, Copy, Paste (at cursor selection point), Duplicate, Delete, (separator), Edit Mesh (should take to Local Geometry but only allow editing of that mesh, wireframe only for that mesh), Mirror (submenus X, Y, Z), Rotate (at 90* increments in 6 directions, should be relative to camera angle/position similar to arrow keys and page up mapping), (separator), Move to Chunk (selection based on workspace).

1b: Selecting global instantiated object(s): Cut, Copy, Paste (at cursor selection point), Duplicate, Delete, (separator), Edit Global Object (finds that object in the global object manager and views it in edit mode), Hide/Show Centroid, (separator), Mirror (same as before), Rotate (same as before), (separator), Move to Chunk (same as before), Move to Prefix (should stage it in global geometry viewport). 

1c: Selecting face(s) (in Local Geometry's Face mode): Subdivide, Triangulate (turns into its triangles), Connect (if multiple faces are selected and either their textures are contiguous to form a quadrilateral which is rare, or if there is no texture the preferred route), (separator), Delete (two submenu options: Delete Face, Delete Face and Vertices, where codependent vertices will be deleted), (separator), (separator), Extrude, Skew, (separator), Mirror Texture (submenus for horizontal/vertical each way), Rotate Texture (90*, 180*, 270*), Revert Texture to Original (should be aware of its original mapping if faces/vertices remapped, should also see if texture is still the same on original, maybe including this is too convoluted and adds too much method??), Remove Texture.

1d: Selecting vertex/vertices (in Local Geometry's Vertex mode): Add Face (only selectable if 3-4 non-collinear vertices are selected, greyed out if not), Extrude into Face (only if 2 or more vertices selected, only if vertices are roughly collinear, should be grayed out otherwise), (separator), Cut (will affect all the corresponding faces), Copy, Paste (based on cursor position, requires reindex), Duplicate (not connected to same mesh but new mesh if capacity for it, requires reindexing of new vertices), Delete (same as cut).

1e: Selecting Empty Space (Waypoints viewport): Add Waypoint Here, Paste (Waypoint/Trigger).

1f: Selecting a waypoint: Add Trigger / Link, (separator), Cut, Copy, Paste, Duplicate, Delete, (separator), Teleport Camera to Waypoint, Rotate Waypoint (Submenus: +90, -90, +15, -15), Snap to Floor, (separator), Edit Node Metadata.

1g: Selecting a Trigger / Link (Door/Event Volume): Teleport Camera to Destination, Load Destination Map, (separator), Cut, Copy, Duplicate, Delete, (separator), Quick Convert Type (Submenus: Door Transition, Read Message, Save Menu, Script Event), Resize Volume, Edit Trigger Properties.

2: Chunk Manager (removing the alias textbox in preference of setting aliases through a right-click menu popup with textbox): Add/Remove Chunk to/from Viewport, Add/Remove Chunk to/from Workspace, Add/Remove Chunk to/from Workspace, (separator), Restore to Original (warn), Restore Dependencies to Original (warn), (separator), View Properties (displays mesh/face/vertex and etc counts), View Hex Data (later add a Hex editor), (separator), Verify Integrity, (separator), Cut, Copy, Paste (warn if full, check if there is any selection), Delete (warn), (separator), Bulldoze (warn, should just create a flat plane across bottom, with no textures and only floor collision data).

3. Outliner:
3a: Chunk Root Node: Expand All / Collapse All, View Properties, Focus Camera on Chunk, Unload from Workspace.
3b: Local / Global Geometry Object Node: Select Object, Focus Camera on Object, Duplicate Object, Move Object to Chunk..., Delete Object.
3c: Mesh Node: Select Mesh, Duplicate Mesh, Extract as New Object, Delete Mesh.
3d: GPU Batch Node: Highlight Batch.

4. Textures:
4a: Texture Map (basically the UV map and texture file selector): Reset UV to Original, Save to Recent Tiles, Open in Texture Edit.

4b: Texture Edit: Export TIM as PNG/CLUT, Import PNG/CLUT as TIM, (separator), Replace Selection from Texture, Replace Selection from Custom, (separator), Open in Texture Map. (Note: right-clicking the CLUT palette colour will bring up a hex wheel).

5: Waypoints: Previously discussed in viewport, everything should be in viewport.

6: Maps: Load Map Layout, Delete Waypoint Logic (Revert), Clear Override Deployments, (separator), Deploy Map to Decomp C Source, Export Geometry, Export Textures, View Map Dependencies, Open in Explorer, Change Prefix (warn unless 'unused' is in enum name).