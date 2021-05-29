#!/usr/bin/env bash

#
# Build the Debian artifacts
#
set -xe
sudo apt-get -qq update
sudo apt-get install devscripts equivs

# Build dependencies
mk-build-deps build-deps/control
sudo dpkg -i ./*all.deb   || :
sudo apt-get --allow-unauthenticated install -f

sudo apt-get install libglu1-mesa-dev

# Install pre-built cmake 3.12
repo="https://dl.cloudsmith.io/public/alec-leamas/opencpn-plugins-stable"
wget $repo/raw/names/cmake/versions/3.12-trusty/cmake-3.12.4-trusty.tar.gz
sudo tar -C /usr/local -xf cmake-3.12.4-trusty.tar.gz


# Build the tarball
if [ -n "$BUILD_GTK3" ]; then
    sudo update-alternatives --set wx-config \
        /usr/lib/*-linux-*/wx/config/gtk3-unicode-3.0
fi

mkdir  build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
VERBOSE=1 cmake --build . --target tarball

#  Install cloudsmith-cli, used by upload script.
sudo apt-get install software-properties-common
sudo add-apt-repository -y ppa:deadsnakes/ppa
sudo apt -qq update
sudo apt-get -q install python3.5 python3.5-venv
for py in $(ls /usr/bin/python3.[0-9]); do
    sudo update-alternatives --install /usr/bin/python3 python3 $py 1
done
sudo update-alternatives --set python3 /usr/bin/python3.5

sudo apt install -q \
    python3-pip python3-setuptools python3-dev python3-wheel \
    build-essential libssl-dev libffi-dev
python3 -m ensurepip --upgrade --user
python3 -m pip install -q --user cryptography
python3 -m pip install -q --user cloudsmith-cli

# python install scripts in ~/.local/bin, teach upload.sh to use in it's PATH:
echo 'export PATH=$PATH:$HOME/.local/bin' >> ~/.uploadrc
