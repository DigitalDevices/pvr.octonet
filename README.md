# Octonet PVR
Digital Devices [Octonet](http://www.digital-devices.eu/shop/de/netzwerk-tv/) PVR client addon for [Kodi](http://kodi.tv)

[![License: GPL-2.0-or-later](https://img.shields.io/badge/License-GPL%20v2+-blue.svg)](LICENSE.md)
[![Build and run tests](https://github.com/DigitalDevices/pvr.octonet/actions/workflows/build.yml/badge.svg?branch=Omega)](https://github.com/DigitalDevices/pvr.octonet/actions/workflows/build.yml)
[![Build Status](https://jenkins.kodi.tv/view/Addons/job/DigitalDevices/job/pvr.octonet/job/Omega/badge/icon)](https://jenkins.kodi.tv/blue/organizations/jenkins/DigitalDevices%2Fpvr.octonet/branches/)

# Building

These instructions work on all supported platforms for the most part. Obviously, paths need to be
adjusted according to your OS (`/` vs `\`). We use Linux paths here as an example.

Clone the `pvr.octonet` repository:

```
$ git clone https://github.com/DigitalDevices/pvr.octonet.git
```

Clone the Kodi repository:

```
$ git clone --branch master https://github.com/xbmc/xbmc.git
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
