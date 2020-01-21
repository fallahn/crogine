/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#ifdef __ANDROID__
//implement missing stl functions on android

#pragma once

#include <string>
#include <sstream>
namespace std
{
	template <typename T>
	std::string to_string(T value)
	{
		std::ostringstream os;
		os << value;
		return os.str();
	}

    //merely for base compatibility - does not contain the size nor base params...
    static inline int stoi(const std::string& str)
    {
        int retVal;
        std::istringstream is(str);
        if (is >> retVal) return retVal;
        return 0;
    }
}

//no iostreams for apk so override fopen
//FORGET THIS - USE SDL_RWops and let the magic happen...
//#include <stdio.h>
//#include <android/asset_manager.h>
//
//#ifdef __cplusplus
//extern "C" {
//#endif //__cplusplus
//
//    void android_fopen_set_asset_manager(AAssetManager* manager);
//    FILE* android_fopen(const char* fname, const char* mode);
//
//#define fopen(name, mode) android_fopen(name, mode)
//
//#ifdef __cplusplus
//}
//#endif //__cplusplus

#endif //__ANDROID__