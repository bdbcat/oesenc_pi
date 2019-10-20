#!/usr/bin/env bash

#
# Upload the .tar.gz and .xml artifacts to cloudsmith
#

set -xe

UNSTABLE_REPO='alec-leamas/opencpn-plugins-unstable'
STABLE_REPO='alec-leamas/opencpn-plugins-stable'

if [ -z "$CIRCLECI" ]; then
    exit 0;
fi

if [ -z "$CLOUDSMITH_API_KEY" ]; then
    echo 'Cannot deploy to cloudsmith, missing $CLOUDSMITH_API_KEY'
    exit 0
fi

if pyenv versions 2>&1 >/dev/null; then
    pyenv global 3.7.0
    python -m pip install cloudsmith-cli
    pyenv rehash
elif dnf --version 2>&1 >/dev/null; then
    sudo dnf install python3-pip python3-setuptools
    sudo python3 -m pip install -q cloudsmith-cli
elif apt-get --version 2>&1 >/dev/null; then
    sudo apt-get install python3-pip python3-setuptools
    sudo python3 -m pip install -q cloudsmith-cli
else
    sudo -H python3 -m ensurepip
    sudo -H python3 -m pip install -q setuptools
    sudo -H python3 -m pip install -q cloudsmith-cli
fi

BUILD_ID=${CIRCLE_BUILD_NUM:-1}
commit=$(git rev-parse --short=7 HEAD) || commit="unknown"
tag=$(git tag --contains HEAD)

tarball=$(ls $HOME/project/build/*.tar.gz)
xml=$(ls $HOME/project/build/*.xml)
test -z "$tag" || sudo sed -i -e "s|$UNSTABLE_REPO|$STABLE_REPO|" $xml

source $HOME/project/build/pkg_version.sh
test -n "$tag" && VERSION="$tag" || VERSION="${VERSION}+${BUILD_ID}.${commit}"
test -n "$tag" && REPO="$STABLE_REPO" || REPO="$UNSTABLE_REPO"

cloudsmith push raw \
    --republish \
    --no-wait-for-sync \
    --name oesenc-${PKG_TARGET}-${PKG_TARGET_VERSION}-metadata \
    --version ${VERSION} \
    --summary "oesenc opencpn plugin metadata for automatic installation" \
    $REPO $xml
cloudsmith push raw  \
    --republish \
    --no-wait-for-sync \
    --name oesenc-${PKG_TARGET}-${PKG_TARGET_VERSION}-tarball \
    --version ${VERSION} \
    --summary "oesenc opencpn plugin tarball for automatic installation" \
    $REPO $tarball
