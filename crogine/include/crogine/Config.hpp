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

#pragma once

#include <cstdint>

#if defined __APPLE__
#include <TargetConditionals.h>
#define GL41 //load GL4.1 not 4.6
#endif

#if defined  __ANDROID__ || TARGET_OS_IPHONE
#define PLATFORM_MOBILE 1
#else
#define PLATFORM_DESKTOP 1
#endif //platform check

//check for audio backend defines and define a fallback if there are none
//#if defined(PLATFORM_MOBILE) && !defined(AL_AUDIO)
//#define SDL_AUDIO 1
//#elif defined(PLATFORM_DESKTOP) && !defined(SDL_AUDIO)
#if !defined(AL_AUDIO) || !defined (SDL_AUDIO)
#define AL_AUDIO 1
#endif

//check which platform we're on and create export macros as necessary
#if !defined(CRO_STATIC)

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef CRO_BUILD

//windows compilers need specific (and different) keywords for export
#define CRO_EXPORT_API __declspec(dllexport)
#define IMGUI_API      __declspec(dllexport)

#else

#define CRO_EXPORT_API __declspec(dllimport)
#define IMGUI_API      __declspec(dllimport)

#endif //BUILD_CRO


//for vc compilers we also need to turn off this annoying C4251 warning
#ifdef _MSC_VER
#pragma warning(disable: 4251)
#endif //_MSC_VER

#else //linux, FreeBSD, Mac OS X

#if __GNUC__ >= 4

//gcc 4 has special keywords for showing/hiding symbols,
//the same keyword is used for both importing and exporting
#define CRO_EXPORT_API __attribute__ ((__visibility__ ("default")))
#define IMGUI_API __attribute__ ((__visibility__ ("default")))

#else

//gcc < 4 has no mechanism to explicitly hide symbols, everything's exported
#define CRO_EXPORT_API
#define IMGUI_API
#endif //__GNUC__

#endif //_WIN32

#else

//static build doesn't need import/export macros
#define CRO_EXPORT_API
#define IMGUI_API

#endif //CRO_STATIC

//used specifically for the Systems
//which implements parallel execution
//this *doesn't* mean anything is thread-safe
//outside of its defined scope, if anything 
//it's probably worse :)
//#define PARALLEL_GLOBAL_DISABLE
#ifndef PARALLEL_GLOBAL_DISABLE
#define USE_PARALLEL_PROCESSING
#endif

#ifdef USE_PARALLEL_PROCESSING
#define EARLY_OUT return
#else
#define EARLY_OUT continue
#endif


#include <crogine/android/Android.hpp>