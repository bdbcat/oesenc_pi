Windows
-------

You will need the general preconditions for building OpenCPN from:
https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:developer_manual:developer_guide:compiling_windows
Setup your build environment according to these instructions
and build OpenCPN like:

#### NMake package
  1. Open the *x86 Native Tools Command Prompt for VS 2017*, and navigate
     to the Builds directory e.g. `C:\Builds\`.
  2. Clone the source code:

          git clone https://github.com/bdbcat/oesenc_pi.git

  3. CD into the new directory `C:\Builds\oesenc_pi\` and create a new
     directory \build\ `mkdir build`
  4. CD into the \build\ directory and enter: (Mind the dots!)

            > cmake -T v141_xp ..

  5. Open the oesenc\_pi.sln in VS2017 to build for Release and make a
     package or build by cmake from the command line:

            > cmake --build .

     Build release version from the command line

            > cmake --build . --config release

     Build setup package and installer metadata/tarball from the command
     line

           > cmake --build . --config release --target package

The Windows install package will be built and given a name like
`oesenc_pi-X.X.xxxx-ov50-win32.exe` together with the installer metadata
(`*.xml`) and tarball (`*.tar.gz`)


Debian
------
First, install the dependencies:

    $ sudo apt install devscripts equivs
    $ mk-build-deps ci/control
    $ sudo apt-get install  ./*all.deb
    $ sudo apt-get --allow-unauthenticated install -f

Build using:

    $ rm -rf build; mkdir build;  cd build
    $ cmake -DOCPN_CI_BUILD=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
    $ make
    $ make package

The debian install package will be built with a name ending in `.deb`.
Install it using `sudo apt install ./*.deb`.

The build also creates artifacts for the new plugin installer: an `*.xml`
metadata file and a `*.tar.gz` tarball


Flatpak
-------

Before build, install flatpak and flatpak-builder as described in
https://flatpak.org/setup/. You also need the *make* and *tar* packages,
both of which ubiquotious on linux.

Then, first setup flatpak:

    $ flatpak remote-add --user --if-not-exists flathub \
          https://flathub.org/repo/flathub.flatpakrepo
    $ flatpak install --user  -y \
          http://opencpn.duckdns.org/opencpn/opencpn.flatpakref
    $ flatpak install --user -y flathub org.freedesktop.Sdk//18.08

Build using:

    $ rm -rf build; mkdir build; cd build
    $ cmake -DOCPN_CI_BUILD=ON -DOCPN_FLATPAK=ON ..
    $ make
    $ make package

The build only creates the installer artifacts: an `*.xml` metadata file and
a `*.tar.gz` tarball. The tarball can be installed locally using the
*ocpn-install.sh* script from the plugins project at
https://github.com/OpenCPN/plugins/tree/master/tools.


MacOS
-----

If not already done, install homebrew as described in
https://docs.brew.sh/Installation

Install the dependencies:

    $ brew install cairo
    $ brew install cmake
    $ brew install libarchive
    $ brew install libexif
    $ brew install python3
    $ brew install wget

Install precompiled wxWidgets 3.1:

    $ pushd /tmp
    $ wget http://opencpn.navnux.org/build_deps/wx312_opencpn50_macos109.tar.xz
    $ tar xJf wx312_opencpn50_macos109.tar.xz
    $ popd

Adjust environment:

    $ export PATH="/usr/local/opt/gettext/bin:$PATH"
    $ echo 'export PATH="/usr/local/opt/gettext/bin:$PATH"' >> ~/.bash_profile

And build:

    $ rm -rf build && mkdir build && cd build
    $ cmake \
      -DwxWidgets_CONFIG_EXECUTABLE=/tmp/wx312_opencpn50_macos109/bin/wx-config \
      -DwxWidgets_CONFIG_OPTIONS="--prefix=/tmp/wx312_opencpn50_macos109" \
      -DOCPN_USE_LIBCPP=ON \
      -DOCPN_CI_BUILD=ON \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 \
      -DCMAKE_INSTALL_PREFIX= "/" \
      ..
    $ make -sj2
    $ make package

The build creates artifacts for the new plugin installer: an `*.xml`
metadata file and a `*.tar.gz` tarball. The tarball can be installed
locally using the *ocpn-install.sh* script from the plugins project at
https://github.com/OpenCPN/plugins/tree/master/tools.

The build also produces a .sh archive; it's usage is at the time of writing
unclear.
