[![Github Actions](https://github.com/fallahn/crogine/actions/workflows/cmake.yml/badge.svg)](https://github.com/fallahn/crogine/actions) 


CROGINE
-------

CROssplatform enGINE.

SDL2 Based game engine which runs on Windows, linux, macOS and Android. Compatibility with macOS is pretty good despite Apple's efforts to kill off OpenGL. Hardware availability has prevented testing on iOS. Presumably consoles supported by SDL2 (such as Nintendo switch) are also compatible, possibly with a bit of work.


#### Building
Using the CMake file included in the root directory first generate project files for your compiler/environment of choice, then build and install the crogine library. Samples each have their own cmake file to build them independently. The included findCROGINE.cmake file should find the installed library if it was installed in the default location - else you need to manually point CMake to the crogine lib.

On windows you can use the included Visual Studio 2022 solution to build crogine and the demo projects for both Windows, and Android if the cross platform tools for Visual Studio are installed. To get started with building Android projects with Visual Studio see [this repository](https://github.com/fallahn/sdl2vs) which includes a sample SDL2 application for Android.

Android make files can also be created with the included cmake file, but the toolchain will first need configuring in cmake/toolchains/android-arm.cmake

## Linux

Install the required dependencies

```
sudo apt install libfreetype-dev libsdl2-dev libopenal-dev libbullet-dev libopus-dev libunwind-dev libtbb-dev libsqlite3-dev libcurl4-openssl-dev
```

Build with cmake

```
mkdir build
cd build
cmake -DBUILD_SAMPLES=ON ../
make
```

#### About
crogine was built with the aim of creating a flexible ECS based framework to run on mobile devices which support OpenGLES2. crogine supplies renderers for both 2D and 3D GLES2 based graphics, although desktop builds provide an OpenGL 4.6 context (GL 4.1 on macOS). As crogine is targeted at lower end hardware it doesn't feature a huge array of cutting edge rendering techniques - however desktop builds do support some PBR rendering, and new features are still being added.  

crogine uses OpenAL for audio, and SDL2 for cross platform parts such as windowing, events and context creation. Documentation can be generated with doxygen using the doxy file in common/docs. Eventually I plan to fully document features on the github wiki as and when I have the time. It might be worth noting that the API is very similar to that of my other library [xygine](https://github.com/fallahn/xygine) so studying that may be of some use.

Included systems can be used to create and render 3D scenes, 2D scenes (using sprites), rendering text, particles and playing back 3D audio. The API is designed specifically for creating and extending the ECS for custom use as quickly and easily as possible. See the [samples](https://github.com/fallahn/crogine/tree/master/samples) directory for more detail.

The `extras` directory contains the source for useful systems such as collision detection, which aren't included in the main library.

Note that for the last few years Android development has fallen somewhat behind and is unlikely to build as-is. However I've tried to keep as many recent features as compatible as possible so with a little work it's not unreasonable to assume that it can be brought up to date.

Portions of this software are copyright (c) 2019 The FreeType Project (www.freetype.org). All rights reserved.  
Portions of this software are copyright (c) 2019 SFML (www.sfml-dev.org). All rights reserved.

-----------------------------------------------------------------------

Matt Marchant 2017 - 2024  
http://trederia.blogspot.com  

crogine - Zlib license.  

This software is provided 'as-is', without any express or  
implied warranty. In no event will the authors be held  
liable for any damages arising from the use of this software.  

Permission is granted to anyone to use this software for any purpose,  
including commercial applications, and to alter it and redistribute  
it freely, subject to the following restrictions:  

1. The origin of this software must not be misrepresented;  
you must not claim that you wrote the original software.  
If you use this software in a product, an acknowledgment  
in the product documentation would be appreciated but  
is not required.  

2. Altered source versions must be plainly marked as such,  
and must not be misrepresented as being the original software.  

3. This notice may not be removed or altered from any  
source distribution.  

-----------------------------------------------------------------------
