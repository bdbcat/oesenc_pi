#!/bin/sh  -xe

#
# Build the Android artifacts inside the circleci linux container
#


set -xe

echo "I am here..."
pwd

cd oesenc_pi
mkdir -p build_android_64_ci
cd build_android_64_ci

#sudo chown -R 1000 /oesenc_pi/build_android_64_ci

#ls -la

sudo apt-get -q update
sudo apt-get -y install git cmake gettext

pwd

ls -la

sudo cmake  \
  -D_wx_selected_config=androideabi-qt \
  -DwxQt_Build=build_android_release_64_static_O3 \
  -DwxQt_Base=/home/dsr/Projects/wxqt/wxWidgets \
  -DQt_Base=/home/dsr/Projects/qt5 \
  -DQt_Build=build_arm64/qtbase \
  -DCMAKE_AR=/opt/android/android-ndk-r20/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android-ar \
  -DCMAKE_CXX_COMPILER=/opt/android/android-ndk-r20/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang++ \
  -DCMAKE_C_COMPILER=/opt/android/android-ndk-r20/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang \
  -DOCPN_Base=/home/dsr/Projects/opencpn \
  -DOCPN_Build=build_android_64 \
  -DPREFIX=/ \
  .. 
  
#sudo make clean  
sudo make
sudo make package

ls -l 

#source ../ci/commons.sh
#repack oesenc_pi-4.2.14-0_android-arm64-16.tar.gz metadata.xml

xml=$(ls *.xml)
tarball=$(ls *.tar.gz)
tarball_basename=${tarball##*/}

echo $xml
echo $tarball
echo $tarball_basename
sudo sed -i -e "s|@filename@|$tarball_basename|" $xml




tmpdir=repack.$$
sudo rm -rf $tmpdir && sudo mkdir $tmpdir
sudo tar -C $tmpdir -xf oesenc_pi-4.2.14-0_android-arm64-16.tar.gz
sudo cp oesenc-plugin-android-arm64-16.xml metadata.xml
sudo cp metadata.xml $tmpdir
sudo tar -C $tmpdir -czf oesenc_pi-4.2.14-0_android-arm64-16.tar.gz .
sudo rm -rf $tmpdir
    


