### Checkout the code

```
git clone --recursive https://github.com/google/dive.git
```

If the code has been checked out without `--recursive` or you're pulling from the main branch later, please run following command to make sure the submodules are retrieved properly.
```
git submodule update --init --recursive
```

### Prerequisite

 - The QT framework, can be installed from [QT online installer](https://download.qt.io/archive/online_installers/4.6/). We are currently using QT 5.11.2
 - gRPC [dependencies](https://github.com/grpc/grpc/blob/master/BUILDING.md#pre-requisites)
 - Android NDK (currently we are using 25.2.9519653). Set the `ANDROID_NDK_HOME` environment variable.
  ```
    export ANDROID_NDK_HOME=~/android_sdk/ndk/25.2.9519653
  ``` 
 - Mako Templates for Python: can be installed with following commandline
  ```
    pip install Mako
  ```
 - gfxreconstruct [dependencies](https://github.com/LunarG/gfxreconstruct/blob/dev/BUILD.md#android-development-requirements), if targetting Android

### Building Dive host tool on Linux
```
# Assumes QTDIR is set to gcc_64 directory (eg. ~/Qt/5.11.2/gcc_64)
export CMAKE_PREFIX_PATH=$QTDIR
export PATH=$QTDIR:$PATH
cd <dive_path>
git submodule update --init --recursive
mkdir build
cd build
cmake -GNinja ..
ninja
```

You can specify the build type for debug/release as well
with `-DCMAKE_BUILD_TYPE=Debug` or `-DCMAKE_BUILD_TYPE=Release` when running cmake.

### Building Dive host tool on Windows
To build with prebuilt gRPC libraries: 
```
REM Assumes QTDIR is set to msvc directory (eg. C:\Qt\5.11.2\msvc2017_64)
set CMAKE_PREFIX_PATH=%QTDIR%
set PATH=%QTDIR%\bin;%PATH%
cd <dive_path>
git submodule update --init --recursive
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
```

Open `dive.sln` and build in Debug or Release 
Or run:
```
cmake --build . --config Debug
cmake --build . --config Release
```
#### Build without prebuilt libraries

Or you can build without using the prebuilt libraries with `-DBUILD_GRPC=ON`

```
cmake -G "Visual Studio 17 2022" -DBUILD_GRPC=ON ..
```

Or you can build with Ninja, which is much faster:
Open Developer Command Prompt for VS 2022(or 2019) and run:
```
cd build
cmake -G "Ninja" -DBUILD_GRPC=ON ..
ninja
```

Note this requires build dependencies for gRPC, which requires to install prerequisite listed at [gRPC website](https://github.com/grpc/grpc/blob/master/BUILDING.md#windows). It's mostly about install the [NASM](https://www.nasm.us/). If you don't have `choco` installed, you can download the binary from [NASM website](https://www.nasm.us/) and add its path to the `PATH` environment variable. 

#### Update prebuilt libraries
Currently, the gRPC binaries are prebuilt under the folder `prebuild``. In case there's build error with the prebuilt libraries, you can run regenerate the libraries with following steps:
 - Open Developer Command Prompt for VS 2022(or 2019)
 - Run `scripts/build_grpc.bat`


### Building Android Libraries

- Download the Android NDK (e.g. 25.2.9519653)
- Set the environment variable `ANDROID_NDK_HOME` (e.g. export ANDROID_NDK_HOME=~/andriod_sdk/ndk/25.2.9519653)
Run the script 

On Linux, run: 
```
./scripts/build_android.sh Debug
```
And on Windows, Open Developer Command Prompt for VS 2022(or 2019) and run 

```
scripts\build_android.bat Debug
```

It will build the debug version of the libraries under `build_android` folder. To build release version, replace parameter with `Release`. To build both versions, do not pass a parameter.

It will also trigger gradle to rebuild gfxreconstruct binaries under `third_party/gfxreconstruct/android/...` and copy them to under `build_android`.

Troubleshooting tips:
- Open the gradle project at `third_party/gfxreconstruct/android` in Android Studio and try making recommended changes to the project and building from there.
- Delete build folders for a clean build
  - `third_party/gfxreconstruct/android/layer/build`
  - `third_party/gfxreconstruct/android/tools/replay/build`
  - `build_android`
- If incremental builds are slow, try building only one version (Debug or Release) and not both

### CLI Tool for capture and cleanup
#### Capture with command line tool for Android applications
The command-line tool currently supports capturing OpenXR and Vulkan applications on Android. To capture, please follow the instructions in the sections above to check out the code and build the Android libraries.

To build the CLI tool on Linux, see [Building Dive host tool on Linux](#building-dive-host-tool-on-linux). You can then use `ninja install` to install the CLI tool under the `install` folder.

To build the CLI tool on Windows, see [Building Dive host tool on Windows](#building-dive-host-tool-on-windows).

To build the Android libraries, see [Building Android Libraries](#building-android-libraries).

Run `./dive_client_cli  --help` for help.
You can find out the device serial by run `adb devices` or by `./dive_client_cli --command list_device`

Examples:
 - Install the dependencies on device and start the package and do a capture after the applications runs 5 seconds.
 ```
 ./dive_client_cli --device  9A221FFAZ004TL --command capture --package de.saschawillems.vulkanBloom --type vulkan --trigger_capture_after 5 --download_path "/path/to/save/captures"
 ```

 - Install the dependencies on device and start the package
 ```
 ./dive_client_cli --device  9A221FFAZ004TL --command run --package com.google.bigwheels.project_04_cube_xr.debug --type openxr --download_path "/path/to/save/captures"
 ```
Then you can follow the hint output to trigger a capture by press key `t` and `enter` or exit by press key `enter` only.

The capture files will be saved at the path specified with the `--download_path` option or the current directory if this option not specified. 

#### Cleanup

The command line tool will clean up the device and application automatically at exit. If somehow it crashed and left the device in a uncleaned state, you can run following command to clean it up

```
./dive_client_cli --command cleanup --package de.saschawillems.vulkanBloom --device 9A221FFAZ004TL
```
This will remove all the libraries installed and the settings that had been setup by Dive for the package.
