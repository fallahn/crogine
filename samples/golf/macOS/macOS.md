# Building a bundle on macOS

Note: if you're planning to just test or develop for macOS you can build golf the 'regular' way as you would on linux, and launch it from the command line. There's no need to (re)build a bundle each time.

## Libraries

Install Xcode from the App Store, or from <https://xcodereleases.com>

With

```shell
sudo xcode-select -s /Applications/Xcode.app
```

you can select the perfect Xcode version for this project (assuming that's where you put Xcode.app), should you run into problems later on.

Make sure you fully read and agree to the Xcode license if you haven't already done so, with

```shell
sudo xcodebuild -license
```

Open Xcode from the Apps folder once, and select "Install".

Make sure to install SDL2, Bullet Physics 2.88 and freetype with Brew

```shell
brew install bullet sdl2 freetype
```

If you haven't already got CMake installed, install that too with Brew

```shell
brew install cmake
```

## Build and install Crogine

Build and install the crogine library from the crogine directory in this repository:

```shell
cd crogine && mkdir build && cd build
cmake ..
sudo make install
```

## Golf build

When the library has installed change to the golf directory

```shell
cd ../samples/golf
```

And configure CMake

```shell
mkdir build && cd build
cmake .. -D MACOS_BUNDLE=true -G Xcode
```

Alternatively you might have some luck with
```
cmake .. -D MACOS_BUNDLE=true -G Xcode -D CMAKE_C_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc" -D CMAKE_CXX_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++"
```

To build a universal binary include the define (requires cmake 3.18 and higher and Xcode 12)

```shell
-D CMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```

This will create an Xcode project in the current build directory. Invoke cmake to build it:

```shell
sudo cmake --build . --config Release --target install
```

This command requires sudo as it will need to copy the libbullet libraries and modify them as part of the bundle. Golf will now build and Xcode will copy the required libraries and fix them up as appropriate. Make sure to restore proper permission to the finished bundle, else they'll be super users only ;)

```shell
sudo chown -Rv $USER Release
```

Copy to /Applications/golf.app 

Finally, you'll need to add `golf.app` to the firewall and unblock it so that network connections are allowed

```
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add /Applications/golf.app
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --unblockapp /Applications/golf.app
```
