[![Build Status](https://travis-ci.org/fallahn/crogine.svg?branch=master)](https://travis-ci.org/fallahn/crogine)

CROGINE
-------

CROssplatform enGINE.

SDL2 Based game engine which runs on Windows, linux and Android. Due to Apple's attitude towards OpenGL support for macOS and iOS is hit and miss. Mostly miss.

#### Building
Using the CMake file included in the root directory first generate project files for your compiler/environment of choice, then build and install the crogine library. To build the samples set the cmake variables BUILD_BATCAT or BUILD_TL to true. The included findCROGINE.cmake file should find the installed library if it was installed in the default location - else you need to manually point CMake to the crogine lib.

On windows you can use the included Visual Studio 2019 solution to build crogine and the demo projects for both Windows, and Android if the cross platform tools for Visual Studio are installed.

Currently there are no Android.mk files for the android build - if anyone wants to submit a pull request I'll be happy to review :)


#### About
crogine was built with the aim of creating a flexible ECS based framework to run on mobile devices which support OpenGLES2. crogine supplies renderers for both 2D and 3D GLES2 based graphics, although due to its modular design creating renderers which target OpenGL 4.x for desktop systems should be relatively straight forward.  

crogine uses OpenAL for audio, SDL2 for cross platform parts, and bullet 2.x for collision detection. Full physics support will be added as and when I deem necessary - the aforementioned modularity allows for easy integration of future features. Documentation can be generated with doxygen using the doxy file in common/docs. Eventually I plan to fully document features on the github wiki as and when I have the time. 

Portions of this software are copyright (c) 2019 The FreeType Project (www.freetype.org).  All rights reserved.

-----------------------------------------------------------------------

Matt Marchant 2017 - 2020  
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
