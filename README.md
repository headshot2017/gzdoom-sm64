# gzdoom-sm64
A fork of gzdoom 3.2.0 which replaces the player with Mario from Super Mario 64 by using libsm64
![Mario in GZDoom](screenshot.png)

This is still a work in progress.

## Download
* Go to [Actions](https://github.com/headshot2017/gzdoom-sm64/actions) page
* Click the first workflow in the list
* Scroll down to "Artifacts" and click gzdoom-sm64-release. **If you can't click it, login on GitHub**

## Compiling
Create `build` folder and run cmake inside that folder:
```
cmake ..
make
```

## Running the game
You will need a DOOM wad and the Super Mario 64 US ROM (filename sm64.us.z64) in order to play. Should work with some DOOM mods as well
