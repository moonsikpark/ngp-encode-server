# ngp-encode-server

This program receives a rendered novel view from [instant-ngp](https://github.com/NVlabs/instant-ngp)'s NeRF model and encodes it into a video format.

## Requirements

- A C++14 compatable compiler.
- Linux distribution of your choice. Distributions other than Ubuntu 20.04 has not been tested.

## Dependencies

- libavcodec, libavformat, libavutil, libswscale
- freetype2

If you are using Ubuntu 20.04, install the following packages;
```sh
sudo apt install build-essential git libavcodec-dev libavformat-dev libavutil-dev \
                 libswscale-dev libfreetype-dev
```

## Compilation

Begin by cloning this repository and all its submodules using the following command:
```sh
$ git clone --recursive https://github.com/moonsikpark/ngp-encode-server
$ cd ngp-encode-server
```

Then, use CMake to build the project:
```sh
ngp-encode-server$ cmake . -B build
ngp-encode-server$ cmake --build build --config RelWithDebInfo -j $(nproc)
```

If the build succeeds, you can now run the code via the `build/neserver` executable.
