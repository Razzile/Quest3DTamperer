### Fork info
This fork was made for a friend and will not be maintained. The project now uses vcpkg for dependencies. Please run `vcpkg install` before compilation.

----

# Quest3DTamperer
**PLEASE NOTE THAT THIS IS STILL IN VERY EARLY DEVELOPMENT!**

This is a tool made for messing with Quest3D games (mainly Audiosurf) to inspect and hopefully learn about their inner workings.

![grafik](https://user-images.githubusercontent.com/42943070/121410792-40245400-c963-11eb-8816-40b3f8dfd06c.png)

# DISCLAIMER
This tool is not supposed to:
- Enable piracy (There's way better ways for that than using this tool anyway)
- Enable cheating online (The tool can't even do this in its current state)
- Cause thermonuclear war

# Features
- Show information about channel groups (Name, protection and read-only status, file path)
- Remove protection and read-only status from channel groups and save them, allowing them to be opened by the Quest3D editor for inspection or editing.
- Load .cgr files into the game (**NOTE:** Not tested properly, don't count on it working)
- Read (and change) strings of some channels (Support added for the most common ones)
- Read and modify script source code of Lua script channels
- Extract textures
- Ability to write channel groups as [DOT files](https://en.wikipedia.org/wiki/DOT_(graph_description_language)) to visualize them as a digraph

More to come soon-ish.

# Dependencies
- The Quest3D 4.0 SDK (4.2 might work too)
- [UGraphViz](https://github.com/Ubpa/UGraphviz)
- [kiero](https://github.com/Rebzzel/kiero)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [Graphviz](https://gitlab.com/graphviz/graphviz/)
- [Detours](https://github.com/microsoft/Detours)
