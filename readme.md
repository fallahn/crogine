CROGINE
-------

CROssplatform enGINE.

SDL2 Based game engine which runs on Windows, linux and Android.

#### Building
Using the CMake file included in the common directory first generate  
project files for your compiler/environment of choice, then build and  
install the crogine library. To build the testwin project (which is a  
misnomer - it actually targets all desktop platforms supported by SDL2)  
use the CMake file to create build files for your preferred environment.  
The included FindCROGINE.cmake file should find the installed library if  
it was installed in the default location - else you need to manually  
point CMake to the crogine lib.

On windows you can use the included Visual Studio 2017 solution to build  
crogine and the demo projects for both Windows, and Android if the  
cross platform tools for Visual Studio are installed.

Currently there are no Android.mk files for the android build - if  
anyone wants to submit a pull request I'll be happy to review :)


-----------------------------------------------------------------------

Matt Marchant 2017  
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