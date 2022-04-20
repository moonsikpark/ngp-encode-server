# ngp-encode-server

This program's goal is to receive a user's point of view, render the view from [instant-ngp](https://github.com/NVlabs/instant-ngp)'s NeRF model and presents it to the user.

## Requirements

- A C++20 compatable compiler. (GCC 8 or later)
- Linux distribution of your choice. Distributions other than Ubuntu 20.04 has not been tested.

## Dependencies

- libavcodec
- libavformat
- libavutil
- libswscale
- freetype2
- libwebsocketpp
- OpenSSL
- libwebrtc
- libprotobuf

If you are using Ubuntu 20.04, install the following packages;
```sh
sudo apt install build-essential    \
                 cmake              \
                 git                \
                 gcc-10             \
                 libavcodec-dev     \
                 libavformat-dev    \
                 libavutil-dev      \
                 libswscale-dev     \
                 libfreetype-dev    \
                 libwebsocketpp-dev \
                 libssl-dev         \
                 libprotobuf-dev

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

## Author

Moonsik Park, Korea Instutute of Science and Tecnhology - moonsik.park@kist.re.kr
