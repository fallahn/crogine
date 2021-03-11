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

#include <crogine/core/App.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/detail/Assert.hpp>

using namespace cro;

std::int16_t GameController::getAxisPosition(std::int32_t controllerIndex, std::int32_t axis)
{
    CRO_ASSERT(App::m_instance, "No app running");
    //CRO_ASSERT(App::m_instance->m_controllers.find(controllerIndex) != App::m_instance->m_controllers.end(), "Controller not connected");
    if (App::m_instance->m_controllers.count(controllerIndex) == 0) return 0;
    return SDL_GameControllerGetAxis(App::m_instance->m_controllers[controllerIndex], static_cast<SDL_GameControllerAxis>(axis));
}

bool GameController::isButtonPressed(std::int32_t controllerIndex, std::int32_t button)
{
    CRO_ASSERT(App::m_instance, "No app running");
    //CRO_ASSERT(App::m_instance->m_controllers.find(controllerIndex) != App::m_instance->m_controllers.end(), "Controller not connected");
    if (App::m_instance->m_controllers.count(controllerIndex) == 0) return false;
    return (SDL_GameControllerGetButton(App::m_instance->m_controllers[controllerIndex], static_cast<SDL_GameControllerButton>(button)) == 1);
}

bool GameController::isConnected(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    return App::m_instance->m_controllers.count(controllerIndex) != 0;
}