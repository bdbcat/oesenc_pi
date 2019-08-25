#!/bin/sh
PLUGIN="oesenc"

VERSION=1.15-0.beta2
OPTS="override=1;publish=1"
PKG="plugins"

API_BASE="https://api.bintray.com/content/leamas/OpenCPN/$PKG/$VERSION"

if [ -z "$BINTRAY_API_KEY" ]; then
    echo 'Cannot deploy: missing $BINTRAY_API_KEY'
    exit 0
fi

cd build
tarball=$(echo ${PLUGIN}*.tar.gz)
xml=$(echo ${PLUGIN}-plugin*.xml)

set -x
echo "Uploading $tarball"
curl -T $tarball  --user leamas:$BINTRAY_API_KEY "$API_BASE/$tarball;$OPTS"
echo
echo "Uploading $xml"
curl -T $xml  --user leamas:$BINTRAY_API_KEY "$API_BASE/$xml;$OPTS"
echo
