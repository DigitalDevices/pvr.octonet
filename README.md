# Octonet PVR
DigitalDevices [Octonet] (http://www.digital-devices.eu/shop/de/netzwerk-tv/) PVR client addon for [Kodi] (http://kodi.tv)

# Building

## Windows
1. Create a file `project/cmake/addons/addons/kodi.pvr.octonet/kodi.pvr.octonet.txt` containing a
   single line `kodi.pvr.octonet file://C:\some\path`. The path doesn't matter and doesn't need to
   actually exist.

2. Use a shell that has environment variables setup by Visual Studio for the native x86 toolchain.
   Run the following in a new build directory and adjust paths accordingly:

```
cmake -G "NMake Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DADDONS_TO_BUILD="kodi.pvr.octonet" ^
    -DADDON_SRC_PREFIX="path_to_where_kodi.pvr.octonet_is" ^
    -DCMAKE_INSTALL_PREFIX="some_subdirectory" ^
    -DPACKAGE_ZIP=ON ^
    "path_to_kodi\project\cmake\addons"
```

Make sure `ADDON_SRC_PREFIX` points to the parent directory of `kodi.pvr.octonet`.

4. Build the addon by running `nmake`. Run `nmake package-addons` to package a zip file of the
   addon.
