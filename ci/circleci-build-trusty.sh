#!/usr/bin/env bash

#
# Build the Debian artifacts
#
set -xe
sudo apt-get -qq update
sudo apt-get install devscripts equivs

mk-build-deps build-deps/control
sudo dpkg -i ./*all.deb   || :
sudo apt-get --allow-unauthenticated install -f
sudo apt-get install libglu1-mesa-dev 

# Get  reasonably modern cmake which meets the requirements
wget https://cmake.org/files/v3.12/cmake-3.12.0-Linux-x86_64.sh
sudo sh cmake-3.12.0-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# Update python and python-pip to extent possible
sudo apt install python3.5  python3-pip
python3.5 -m pip install --user --force-reinstall pip==20.3.4 setuptools==49.1.3


# Install cloudsmith-cli (for upload) and cryptography (for git-push)
sudo apt remove python3-six python3-colorama python3-urllib3
export LC_ALL=C.UTF-8  LANG=C.UTF-8
# https://github.com/pyca/cryptography/issues/5753 -> cryptography < 3.4
python3.5 -m pip install --user cloudsmith-cli 'cryptography<3.4'


# Build tarball and package
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j $(nproc) VERBOSE=1 tarball
make package


# python install scripts in ~/.local/bin, teach upload.sh to use it in PATH:
echo 'export PATH=$PATH:$HOME/.local/bin' >> ~/.uploadrc

# Both python modules needs 3.5:
sudo ln -sf /usr/bin/python3.5 /usr/bin/python3
