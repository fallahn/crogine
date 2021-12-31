### Building a bundle on macOS

Install xcode from the app store, or the commandline tools.

Make sure to install SDL2, Bullet Physics 2.88 and freetype with brew

    brew install bullet sdl2 freetype

Build and install the crogine library from the crogine directory in this repository:

    cd crogine && mkdir build && cd build
    cmake ..
    sudo make install

When the library has installed change to the golf directory

    cd ../../samples/golf

And configure cmake

    mkdir build && cd build
    cmake .. -DMACOS_BUNDLE=true -GXcode

This will create an xcode project in the current build directory. Invoke cmake to build it:

    sudo cmake --build . --config Release --target install

This command requires sudo as it will need to copy the libbullet libraries and modify them as part of the bundle. Golf will now build and xcode will copy the required libraries and fix them up as appropriate.