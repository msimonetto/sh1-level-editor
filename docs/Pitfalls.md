# Pitfalls to Avoid (from experience)

- Using JSON for large-scale manipulation in C++ is clunky and slows down the editing experience immeasurably. `nlohmann/json` should realistically be used for tracking dependencies/dependents and other config. This should be carefully planned out if actually performed.
- For forks that might steer away from ImGui, avoid using Python and/or Blender. Blender Python API is clunky especially with required PS1 validations, particularly around textures and their CLUT row maps.