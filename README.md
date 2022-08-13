# mizu

This codebase is directly adapted from the [yuzu
emulator](https://github.com/yuzu-emu/yuzu), but is heavily modified to gut all
the kernel / ARM emulation in favor of running natively under the [arm64 Horizon
Linux kernel](https://github.com/kentjhall/horizon-linux) as a persistent
systemd service. This service handles the concurrent execution of necessary
Nintendo Switch system services, GPU emulation, and a loader for Horizon apps.

## WIP

What's here is far from complete or being thoroughly tested. There's a lot of
hamfisted synchronization I added to try and run the Switch services in separate
threads (like real services would), but as a result, I'm fairly certain that
most of it is buggy and/or unoptimized. Generally, the major TODOs include:

- Improve the synchronization situation.

- Ensure multiple Horizon programs can be serviced at once.

- The build system!! It's garbage right now, just one convoluted Makefile and
  every header is a dependency of everything. Ideally I figure out CMake at some
  point.

Once these things are sorted, the main focus should be porting over any
important yuzu features / improvements that have occurred since I ripped their
codebase.

## Build/install

This is tested to build on Debian 11 and Fedora Rawhide (post-Fedora 36).

First install the required dependencies...

for Debian:
```
# apt install cmake qtbase5-dev qtbase5-private-dev qtbase5-dev-tools libglfw3-dev libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavresample-dev libavutil-dev libpostproc-dev libswresample-dev libswscale-dev libopus-dev libusb-1.0-0-dev glslang-tools liblz4-dev libboost-dev nlohmann-json3-dev
```

for Fedora:
```
# dnf install make cmake gcc gcc-c++ qt5-qtbase-devel qt5-qtbase-private-devel glfw-devel.aarch64 ffmpeg-free-devel.aarch64 opus-devel.aarch64 libusb1-devel.aarch64 libusb1.aarch64 glslang-devel.aarch64 lz4-devel.aarch64 boost-devel json-devel
```

Then run:
```
$ make -j$(nproc)
```

And to install:
```
# make install
```
