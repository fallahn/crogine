### Building a bundle on macOS

Install Xcode from the App Store, or from https://xcodereleases.com/

With
    sudo xcode-select -s /Applications/Xcode.app
you can select the perfect Xcode version for this project (assuming that's where you put Xcode.app), should you run into problems later on.

Make sure to install SDL2, Bullet Physics 2.88 and freetype with brew

    brew install bullet sdl2 freetype

Build and install the crogine library from the crogine directory in this repository:

    cd crogine && mkdir build && cd build
    cmake ..
    sudo make install

When the library has installed change to the golf directory

    cd ../samples/golf

And configure cmake

    mkdir build && cd build
    cmake .. -D MACOS_BUNDLE=true -G Xcode

This will create an Xcode project in the current build directory. Invoke cmake to build it:

    sudo cmake --build . --config Release --target install

This command requires sudo as it will need to copy the libbullet libraries and modify them as part of the bundle. Golf will now build and Xcode will copy the required libraries and fix them up as appropriate. Make sure to restore proper permission to the finished bundle, else they'll be super users only ;)
