#!/usr/bin/env zsh

# build crogine
cd ./crogine*/
mkdir build
cd build
cmake ..
sudo -A make install

# build golf
cd ../../../samples/golf
# make sure assets have been copied into golf from latest Windows download
mkdir build
cd build
cmake .. -D MACOS_BUNDLE=true -G Xcode
sudo -A cmake --build . --config Release --target install

# set permissions
sudo -A chown -Rv $USER Release

# ad-hoc code sign
codesign --force -s - Release/golf.app

# replace app
rm -rf /Applications/golf.app
cp -R Release/golf.app /Applications/golf.app