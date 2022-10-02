# libsm64 - Super Mario 64 as a library

The purpose of this project is to provide a clean interface to the movement and rendering
code which was reversed from SM64 by the [SM64 decompilation project](https://github.com/n64decomp/sm64),
so that Mario can be dropped in to existing game engines or other systems with minimal effort.
This project produces a shared library file containing mostly code from the decompilation project,
and loads an official SM64 ROM at runtime to get Mario's texture and animation data, so any project
which makes use of this library must ask the user to provide a ROM for asset extraction.

## Bindings and plugins

- [Rust bindings](https://github.com/nickmass/libsm64-rust)
- [Unity plugin](https://github.com/libsm64/libsm64-unity)
- [Blender add-on](https://github.com/libsm64/libsm64-blender)

## Building on Linux

- Ensure python3 is installed.
- Ensure the SDL2 and GLEW libraries are installed if you're building the test program (on Ubuntu: libsdl2-dev, libglew-dev)
- Run `make` to build
- To run the test program you'll need a SM64 US ROM in the root of the repository with the name `baserom.us.z64`.

## Building on Windows (test program not supported)
- [Follow steps 1-4 for setting up MSYS2 MinGW 64 here](https://github.com/sm64-port/sm64-port#windows), but replace the repository URL with `https://github.com/libsm64/libsm64.git`
- Run `make` to build

## Make targets (all platforms)

- `make lib`: (Default) Build the `dist` directory, containing the shared object or DLL and public-facing header.
- `make test`: Builds the library `dist` directory as well as the test program.
- `make run`: Build and run the SDL+OpenGL test program.

