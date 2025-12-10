# Replay Buffer

## How to use
You first set the output directory (or leave it as the temp folder, works either way) and then go to the main menu
(or pause menu) and click on the mod's icon. From there, you set your settings and for the audio settings, you take a
look at the available audio device IDs by clicking the top right corner button. And then you press save settings, and
start recording!

## Building from source (Windows)
You must first build ffmpeg as static libraries, with NVENC and AMF support if you want (or contact me for a build).
After running `make install` on ffmpeg, create a directory with two directories inside, named `lib` and `include`, and
copy over the relevant files from `/usr/local/lib` and `/usr/local/include`. After that, configure the project with 
```shell
mkdir build
cd build
cmake .. -DFFMPEG_ROOT=<path to previously mentioned directory>
```
and then build and install with `cmake --build .`

## Installation
It's not on the Geode index yet, but hopefully soon.

## Known Issues
- Any platform other than Windows is not supported right now
- It's kinda unoptimised
- The UI sucks

## Credits
Special thanks to foody for the icon