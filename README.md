# Replay Buffer

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