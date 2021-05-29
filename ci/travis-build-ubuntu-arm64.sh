#!/usr/bin/env bash
#
#  Set up a raspbian ubuntu image an run a build within it.
#
set -xe

sudo apt-get -qq update

# Create build script
#
cat >$TRAVIS_BUILD_DIR/build.sh << "EOF"
apt -qq update
apt -y -q install git cmake build-essential \
   cmake gettext wx-common libwxgtk3.0-dev \
    libbz2-dev libcurl4-openssl-dev libexpat1-dev \
    libcairo2-dev libarchive-dev liblzma-dev \
    libexif-dev lsb-release 
apt install -y -q python3-pip 
python3 -m pip install -q --force-reinstall pip==20.3.4 setuptools==49.1.3
python3 -m pip install --user -q cloudsmith-cli cryptography cmake

export PATH=$HOME/.local/bin:$PATH
python3 -m pip install cmake
cd /ci-source
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ..
make tarball
EOF


# Run script in docker
#
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
docker run --privileged -ti \
        -v "$TRAVIS_BUILD_DIR:/ci-source:rw" \
        -e "CLOUDSMITH_STABLE_REPO=$CLOUDSMITH_STABLE_REPO" \
        -e "CLOUDSMITH_BETA_REPO=$CLOUDSMITH_BETA_REPO" \
        -e "CLOUDSMITH_UNSTABLE_REPO=$CLOUDSMITH_UNSTABLE_REPO" \
        -e "CIRCLE_BUILD_NUM=$CIRCLE_BUILD_NUM" \
        -e "TRAVIS_BUILD_NUMBER=$TRAVIS_BUILD_NUMBER" \
        $DOCKER_IMAGE /bin/bash -xe /ci-source/build.sh
rm -f $TRAVIS_BUILD_DIR/build.sh


# Install cloudsmith-cli (for upload) and cryptography (for git-push)
#
sudo apt install python3-pip
python3 -m pip install -q --force-reinstall pip==20.3.4 setuptools==49.1.3
sudo apt remove python3-six python3-colorama python3-urllib3
export LC_ALL=C.UTF-8  LANG=C.UTF-8
# https://github.com/pyca/cryptography/issues/5753 -> cryptography < 3.4
python3 -m pip install --user cloudsmith-cli 'cryptography<3.4'

# Make sure the upload script can locate the build dir:
ln -s $TRAVIS_BUILD_DIR/build . || :
