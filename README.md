# ZibraVDB for Houdini
Houdini plugin for ZibraVDB integration. Enables compression/decompression of volumetric data in Houdini.

## Features
* Volumetric compression and decompression
* File cache for volumetrics

Note that compression/decompression implementation is not included in this repository, and is not open source. License from this repository is not applicable to compression/decompression implementation.

## Requirements
* OS: Windows 10 or newer
  * Support for Linux/macOS will be added later
* D3D12 capable GPU recommended

## Building
Before you can build ZibraVDBForHoudini, you need to set up following in your build environment:
* Software
    * Visual Studio 2022
        * C++ support
    * CMake 3.25 or newer
* Environment variables
    * `ZIBRA_HOUDINI_PATH` or `HFS` - must be set to Houdini path (e.g. "C:/Program Files/Side Effects Software/Houdini 20.5.386")
        * If both are set, `ZIBRA_HOUDINI_PATH` takes precedence

After setting up build environment you can do one of the following:
1. Build via build script
    * Run ./scripts/build.ps1
2. Build manually
    * In the root of the repository, run following commands: 
        * `cmake -S . -B build`
        * `cmake --build build --config Release`

After successful build, plugin will be automatically installed into HOUDINI_USER_PREF_DIR and can be used in Houdini

## License

See [LICENSE](LICENSE) for details. Note that it is only applicable for the open source part of ZibraVDB for Houdini. It does not cover implementation of compression/decompression.

## Contributing 

See [CONTRIBUTING.md](CONTRIBUTING.md) for details.
