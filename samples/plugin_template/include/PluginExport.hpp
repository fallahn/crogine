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

#pragma once

#if defined(_WIN32)

//windows compilers need specific (and different) keywords for export
#define CROPLUG_EXPORT_API __declspec(dllexport)

//for vc compilers we also need to turn off this annoying C4251 warning
#ifdef _MSC_VER
#pragma warning(disable: 4251)
#endif //_MSC_VER

#else //linux, FreeBSD, macOS

#if __GNUC__ >= 4

//gcc 4 has special keywords for showing/hiding symbols,
//the same keyword is used for both importing and exporting
#define CROPLUG_EXPORT_API __attribute__ ((__visibility__ ("default")))
#else

//gcc < 4 has no mechanism to explicitly hide symbols, everything's exported
#define CROPLUG_EXPORT_API
#endif //__GNUC__

#endif //_WIN32

namespace cro
{
    class StateStack;
}

#include <limits>
#include <cstdint>
#include <any>

namespace StateID
{
    //push this state to the stack to return to the front-end
    //REMEMBER TO CLEAR THE STACK FIRST!
    static constexpr std::int32_t ParentState = std::numeric_limits<std::int32_t>::max();
}

#ifdef __cplusplus
extern "C" 
{

#endif

    /*!
    \brief Entry point for game plugins
    Use this function to register your game states with
    the state stack passed via the pointer.
    \param stateStack Pointer to the state stack instance with which
    to register your game states
    \param sharedData Pointer to a std::any object which can be used to share
    data between your states. This can be forwarded to the constructor of any
    of your state class when registering the state

    stateStack->registerState<MenuState>(StateID::MainMenu, *sharedData);

    The shared data must meet the requirements of std::any. The pointer is
    guaranteed by the front-end to point to a valid, empty std::any instance.

    \returns The state ID which should be pushed onto the stack when it is launched
    */
    CROPLUG_EXPORT_API int begin(cro::StateStack* stateStack, std::any* sharedData);

    
    /*!
    \brief Called by OSGC when a plugin is unloaded.
    At the very least this needs to unregister ALL states which
    were registered with the state stack during begin
    
    stateStack->unregisterState(StateID::MainMenu);

    */
    CROPLUG_EXPORT_API void end(cro::StateStack* stateStack);

#ifdef __cplusplus
}
#endif