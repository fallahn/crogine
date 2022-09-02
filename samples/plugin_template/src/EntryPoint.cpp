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

#include "PluginExport.hpp"
#include "StateIDs.hpp"
#include "MenuState.hpp"

#include <crogine/core/StateStack.hpp>
#include <crogine/core/Log.hpp>


int begin(cro::StateStack* ss, std::any* sharedData)
{
    /*
    sharedData is a pointer to a std::any whose lifetime is
    managed by cro::App. You should initialise it here with
    std::make_any if you wish to create a shared data object
    with which to share information between any states registered here

    eg:
    *sharedData = std::make_shared<MyType>();
    
    MyType& mySharedInstance = std::any_cast<MyType&>(*sharedData);
    ss->registerState<SomeState>(StateID::SomeState, mySharedInstance);
    ss->registerState<SomeOtherState>(StateID::SomeOtherState, mySharedInstance);

    mySharedInstance will then be passed to the State's constructor each
    time the state is pushed on to the state stack.

    When plugins are unloaded the shared data object is reset.
    */

    ss->registerState<MenuState>(StateID::MainMenu);
    return StateID::MainMenu;
}

void end(cro::StateStack* ss)
{
    ss->unregisterState(StateID::MainMenu);
}