Plugin Project Template
-----------------------

This folder contains the template files required to create a crogine plugin. The source is licensed under the zlib license.


Plugins allow loading and running external shared libraries which contain States that can be executed by crogine. Once a plugin is built it can be loaded by a crogine application with `cro::App::loadPlugin()` and similarly unloaded with `cro::App::unloadPlugin()`. Each function takes a reference to a StateStack with which the plugin states are registered. See EntryPoint.cpp and the ScratchPad project in the `samples` folder of the repository for an example of how loading and unloading a plugin works.

/*********************************************************************  
(c) Matt Marchant 2022  
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
*********************************************************************/  