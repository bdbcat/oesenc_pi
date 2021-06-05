# oesenc\_pi README

This is a plugin for [opencpn](https://www.opencpn.org ) providing support
for encrypted charts available from [o-charts.org](http://o-charts.org)
The plugin supports purchase, downloading and rendering of these charts.

More user info: https://opencpn.org/OpenCPN/plugins/oesenc.html

The plugin uses a continous integration setup described in the
[wiki](https://github.com/Rasbats/managed_plugins/wiki/Alternative-Workflow)

## Building

### General

Install build dependencies as described in the
[wiki](https://github.com/Rasbats/managed_plugins/wiki/Local-Build)
Then clone this repository, enter it and make
`rm -rf build; mkdir build; cd build`.

A tar.gz tarball which can be used by the new plugin installer, available
from OpenCPN 5.2.0 is built using:

    $ cmake ..
    $ make tarball

To build the flatpak tarball:

    $ cmake ..
    $ make flatpak

On most platforms besides flatpak: build a platform-dependent legacy
installer like a NSIS .exe on Windows, a Debian .deb package on Linux
and a .dmg image for MacOS:

    $ cmake ..
    $ make pkg

#### Building on windows (MSVC)
On windows, a somewhat different workflow is used:

    > cmake -T v141_xp -G "Visual Studio 15 2017" --config RelWithDebInfo  ..
    > cmake --build . --target tarball --config RelWithDebInfo

This is to build the installer tarball. Use _--target pkg_ to build the
legacy NSIS installer.

## Plugin Catalog Git Push Integration

The build system is able to push all metadata changes to a local clone
of the plugins catalog project at https://github.com/opencpn/plugins.
The purpose is to make it easier to make pull requests to update the
catalog. See
https://github.com/Rasbats/managed_plugins/wiki/OpenCPN-Plugins-Catalog-and-Github-Integration


## Copyright and licensing

This software is copyright (c) David Register 2020. It is distributed under
the terms of the Gnu Public License version 2 or, at your option, any later
version. See COPYING.gplv2.

The oeserverd binary and libsgllnx64-2.29.02 libraries are closed source.
Re-distribution of these items is allowed.

The sources also contains dependencies distributed under various open-source
licenses including Expat, the Curl license, SGI-B, Zlib and Khronos. Refer
to the source files for details.
