# Octonet PVR
Digital Devices [Octonet](http://www.digital-devices.eu/shop/de/netzwerk-tv/) PVR client addon for [Kodi](http://kodi.tv)

| Platform | Status |
|----------|--------|
| Linux + OS X (Travis) | [![Build Status](https://travis-ci.org/julianscheel/pvr.octonet.svg?branch=master)](https://travis-ci.org/julianscheel/pvr.octonet) |
| Windows (AppVeyor) | [![Build status](https://ci.appveyor.com/api/projects/status/m7dhmpmuf5coir5h?svg=true)](https://ci.appveyor.com/project/julianscheel/pvr-octonet) |

# Building

These instructions work on all supported platforms for the most part. Obviously, paths need to be
adjusted according to your OS (`/` vs `\`). We use Linux paths here as an example.

Clone the `pvr.octonet` repository:

```
$ git clone https://github.com/DigitalDevices/pvr.octonet.git
```

Clone the Kodi repository:

```
$ git clone --branch Matrix https://github.com/xbmc/xbmc.git
```

```
$ cd pvr.octonet
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release -DADDONS_TO_BUILD="pvr.octonet" -DADDON_SRC_PREFIX="path to parent of pvr.octonet" -DCMAKE_INSTALL_PREFIX="install" -DPACKAGE_ZIP=ON "path to kodi/cmake/addons"
```

On Windows, you should add `-G "NMake Makefiles"` to the CMake invocation. Make sure that
`ADDON_SRC_PREFIX` does _not_ point directly to `pvr.octonet` but instead to its parent directory.

Finally, build the plugin with `make` (or `nmake` on Windows). The plugin should be in an `install`
subdirectory.
