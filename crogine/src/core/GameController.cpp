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
    if (App::m_instance->m_controllers.count(controllerIndex) == 0) return 0;
    return SDL_GameControllerGetAxis(App::m_instance->m_controllers.at(controllerIndex).controller, static_cast<SDL_GameControllerAxis>(axis));
}

bool GameController::isButtonPressed(std::int32_t controllerIndex, std::int32_t button)
{
    CRO_ASSERT(App::m_instance, "No app running");
    if (App::m_instance->m_controllers.count(controllerIndex) == 0) return false;
    return (SDL_GameControllerGetButton(App::m_instance->m_controllers.at(controllerIndex).controller, static_cast<SDL_GameControllerButton>(button)) == 1);
}

bool GameController::isConnected(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    return App::m_instance->m_controllers.count(controllerIndex) != 0;
}

bool GameController::hasHapticSupport(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    return App::m_instance->m_controllers.count(controllerIndex) != 0 && App::m_instance->m_controllers.at(controllerIndex).haptic != nullptr;
}

HapticEffect GameController::registerHapticEffect(std::int32_t controllerIndex, SDL_HapticEffect& effect)
{
    CRO_ASSERT(App::m_instance, "No app running");

    HapticEffect retVal;
    retVal.controllerIndex = controllerIndex;

    if (App::m_instance->m_controllers.count(controllerIndex) == 0)
    {
        LogE << "Unable to register haptic effect: controller index " << controllerIndex << " doesn't exist";
        return retVal;
    }

    const auto& controller = App::m_instance->m_controllers.at(controllerIndex);
    if (controller.haptic == nullptr)
    {
        LogE << "Unable to register haptic effect: controller index " << controllerIndex << " doesn't support haptics";
        return retVal;
    }

    retVal.effectID = SDL_HapticNewEffect(controller.haptic, &effect);
    if (retVal.effectID < 0)
    {
        auto* error = SDL_GetError();
        LogE << error << " for controller " << controllerIndex << std::endl;
        return retVal;
    }

    return retVal;
}

void GameController::startHapticEffect(HapticEffect effect, std::uint32_t repeat)
{
    CRO_ASSERT(App::m_instance, "No app running");

    if (App::m_instance->m_controllers.count(effect.controllerIndex))
    {
        if (auto result = SDL_HapticRunEffect(App::m_instance->m_controllers.at(effect.controllerIndex).haptic, effect.effectID, repeat); result < 0)
        {
            auto* error = SDL_GetError();
            LogE << error << " on controller " << effect.controllerIndex << std::endl;
        }
    }
    else
    {
        LogE << "Failed to start haptic effect on " << effect.controllerIndex << ": controller not found" << std::endl;
    }
}

void GameController::stopHapticEffect(HapticEffect effect)
{
    CRO_ASSERT(App::m_instance, "No app running");

    if (App::m_instance->m_controllers.count(effect.controllerIndex))
    {
        if (auto result = SDL_HapticStopEffect(App::m_instance->m_controllers.at(effect.controllerIndex).haptic, effect.effectID); result < 0)
        {
            auto* error = SDL_GetError();
            LogE << error << " on controller " << effect.controllerIndex << std::endl;
        }
    }
    else
    {
        LogE << "Failed to stop haptic effect on " << effect.controllerIndex << ": controller not found" << std::endl;
    }
}

bool GameController::isHapticActive(HapticEffect effect)
{
    CRO_ASSERT(App::m_instance, "No app running");

    if (App::m_instance->m_controllers.count(effect.controllerIndex))
    {
        auto result = SDL_HapticGetEffectStatus(App::m_instance->m_controllers.at(effect.controllerIndex).haptic, effect.effectID);
        if (result < 0)
        {
            auto* error = SDL_GetError();
            LogE << error << " on controller " << effect.controllerIndex << std::endl;
        }

        return result == 1;
    }
    else
    {
        LogE << "Get effect status: controller " << effect.controllerIndex << " not found." << std::endl;
    }

    return false;
}

void GameController::rumbleStart(std::int32_t controllerIndex, float strength, std::uint32_t duration)
{
    CRO_ASSERT(App::m_instance, "No app running");

    if (App::m_instance->m_controllers.count(controllerIndex))
    {
        if (SDL_HapticRumblePlay(App::m_instance->m_controllers.at(controllerIndex).haptic, strength, duration) < 0)
        {
            auto* error = SDL_GetError();
            LogE << error << " on controller " << controllerIndex << std::endl;
        }
    }
}

void GameController::rumbleStop(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");

    if (App::m_instance->m_controllers.count(controllerIndex))
    {
        if (SDL_HapticRumbleStop(App::m_instance->m_controllers.at(controllerIndex).haptic) < 0)
        {
            auto* error = SDL_GetError();
            LogE << error << " on controller " << controllerIndex << std::endl;
        }
    }
}