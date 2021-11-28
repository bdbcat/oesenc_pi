#!/usr/bin/env bash

#
# Build the flatpak artifacts.
#

# Copyright (c) 2021 Alec Leamas
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

set -e


MANIFEST=$(cd flatpak; ls org.opencpn.OpenCPN.Plugin*yaml)
echo "Using manifest file: $MANIFEST"
set -x

# Load local environment if it exists i. e., this is a local build
if [ -f ~/.config/local-build.rc ]; then source ~/.config/local-build.rc; fi
if [ -d /ci-source ]; then cd /ci-source; fi

# Set up build directory and a visible link in /
builddir=build-flatpak
test -d $builddir || sudo mkdir $builddir && sudo rm -rf $builddir/*
sudo chmod 777 $builddir
if [ "$PWD" != "/"  ]; then sudo ln -sf $PWD/$builddir /$builddir; fi

# Create a log file.
exec > >(tee $builddir/build.log) 2>&1

if [ -n "$TRAVIS_BUILD_DIR" ]; then cd $TRAVIS_BUILD_DIR; fi

if [ -n "$CI" ]; then
    sudo apt update

    # Avoid using outdated TLS certificates, see #210.
    sudo apt install --reinstall  ca-certificates

    # Install flatpak and flatpak-builder
    sudo apt install flatpak flatpak-builder
fi
flatpak remote-add --user --if-not-exists \
    flathub https://dl.flathub.org/repo/flathub.flatpakrepo

# aarch64 is built from beta branch, regular x86_64 from stable.
# Both uses 20.08. Compatibility 18.08 builds are built for
# x86_64 only using last known 18.08 commit on stable branch.

commit_1808=959f5fd700f72e63182eabb9821b6aa52fb12189eddf72ccf99889977b389447
FLATPAK_BRANCH=stable
if [ "$(uname -m)" = "aarch64" ]; then
    flatpak install --user -y --noninteractive \
        flathub org.freedesktop.Sdk//20.08
    flatpak remote-add --user --if-not-exists flathub-beta \
        https://flathub.org/beta-repo/flathub-beta.flatpakrepo
    flatpak install --user -y --or-update  --noninteractive \
        flathub-beta org.opencpn.OpenCPN
    FLATPAK_BRANCH=beta
elif [ -n "$BUILD_1808" ]; then
    flatpak install --user -y --noninteractive \
        flathub org.freedesktop.Sdk//18.08
    flatpak install --user -y --or-update --noninteractive \
        flathub  org.opencpn.OpenCPN
    flatpak update --user -y --noninteractive --commit $commit_1808 \
        org.opencpn.OpenCPN
    sed -i '/sdk:/s/20.08/18.08/'  flatpak/org.opencpn.*.yaml
else
    flatpak install --user -y --noninteractive \
        flathub org.freedesktop.Sdk//20.08
    flatpak install --user -y --or-update --noninteractive \
        flathub  org.opencpn.OpenCPN
fi

# Patch the runtime version so it matches the nightly builds
# or beta as appropriate.
test -w flatpak/$MANIFEST || sudo chmod go+w flatpak/$MANIFEST
sed -i "/^runtime-version/s/:.*/: $FLATPAK_BRANCH/" flatpak/$MANIFEST

# The flatpak checksumming needs python3:
if ! python3 --version 2>&1 >/dev/null; then
    pyenv local $(pyenv versions | sed 's/*//' | awk '{print $1}' | tail -1)
    cp .python-version $HOME
fi

# Configure and build the plugin tarball and metadata.
cd $builddir
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j $(nproc) VERBOSE=1 flatpak

# Restore permissions and owner in build tree.
if [ -d /ci-source ]; then sudo chown --reference=/ci-source -R . ../cache; fi
sudo chmod --reference=.. .

# Fix upload script if building 18.08, handle possible read-only current dir:
test -w upload.sh || sudo chmod go+w upload.sh
test -n "$BUILD_1808" && sed 's/20.08/18.08/' upload.sh > /tmp/upload.sh \
   && cp /tmp/upload.sh upload.sh && rm /tmp/upload.sh

# Restore patched file so the cache checksumming is ok.
git checkout ../flatpak/$MANIFEST

# Install cloudsmith and cryptography, required by upload script and git-push
python3 -m pip install -q --user --upgrade pip
python3 -m pip install -q --user cloudsmith-cli cryptography

# python install scripts in ~/.local/bin, teach upload.sh to use this in PATH
echo 'export PATH=$PATH:$HOME/.local/bin' >> ~/.uploadrc
