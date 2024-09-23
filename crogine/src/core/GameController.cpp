/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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

std::int32_t GameController::m_lastControllerIndex = 0;

std::int32_t GameController::deviceID(std::int32_t controllerID)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerID < MaxControllers, "");

    return controllerID < 0 ? -1 : App::m_instance->m_controllers[controllerID].joystickID;
}

std::int32_t GameController::controllerID(std::int32_t joystickID)
{
    //hmmmmmmmmm either this or steam is incorrect as using this to query
    //steam for controller type returns the wrong device..
    
    //return SDL_JoystickGetPlayerIndex(SDL_JoystickFromInstanceID(joystickID));
    return SDL_GameControllerGetPlayerIndex(SDL_GameControllerFromInstanceID(joystickID));
}

std::int16_t GameController::getAxisPosition(std::int32_t controllerIndex, std::int32_t axis)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    if (controllerIndex < 0)
    {
        //return the average of all inputs
        std::int32_t sum = 0;
        std::int32_t controllerCount = getControllerCount();

        if (controllerCount == 0)
        {
            return 0;
        }

        //there may be only 2 connected but indexed at 2/3 if 0 and 1 were disconnected mid-game
        for (auto i = 0; i < /*controllerCount*/4; ++i)
        {
            if (App::m_instance->m_controllers[i].controller)
            {
                sum += SDL_GameControllerGetAxis(App::m_instance->m_controllers[i].controller, static_cast<SDL_GameControllerAxis>(axis));
            }
        }
        return static_cast<std::int16_t>(sum / controllerCount);
    }

    return SDL_GameControllerGetAxis(App::m_instance->m_controllers[controllerIndex].controller, static_cast<SDL_GameControllerAxis>(axis));
}

bool GameController::isButtonPressed(std::int32_t controllerIndex, std::int32_t button)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    if (controllerIndex < 0)
    {
        for (auto i = 0; i < /*getControllerCount()*/4; ++i)
        {
            if (SDL_GameControllerGetButton(App::m_instance->m_controllers[i].controller, static_cast<SDL_GameControllerButton>(button)) == 1)
            {
                return true;
            }
        }
        return false;
    }

    return (SDL_GameControllerGetButton(App::m_instance->m_controllers[controllerIndex].controller, static_cast<SDL_GameControllerButton>(button)) == 1);
}

bool GameController::isConnected(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    return controllerIndex < 0 ? false : (App::m_instance->m_controllers[controllerIndex].controller != nullptr);
}

bool GameController::hasHapticSupport(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    return controllerIndex < 0 ? false : (App::m_instance->m_controllers[controllerIndex].haptic != nullptr);
}

HapticEffect GameController::registerHapticEffect(std::int32_t controllerIndex, SDL_HapticEffect& effect)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    HapticEffect retVal;
    if (controllerIndex < 0)
    {
        return retVal;
    }

    if (App::m_instance->m_controllers[controllerIndex].controller == nullptr)
    {
        LogE << "Unable to register haptic effect: controller index " << controllerIndex << " doesn't exist";
        return retVal;
    }

    const auto& controller = App::m_instance->m_controllers[controllerIndex];
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

    retVal.controllerIndex = controllerIndex;
    return retVal;
}

void GameController::startHapticEffect(HapticEffect effect, std::uint32_t repeat)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(effect.controllerIndex < MaxControllers && effect.controllerIndex > -1, "");

    if (App::m_instance->m_controllers[effect.controllerIndex].haptic)
    {
        if (auto result = SDL_HapticRunEffect(App::m_instance->m_controllers[effect.controllerIndex].haptic, effect.effectID, repeat); result < 0)
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
    CRO_ASSERT(effect.controllerIndex < MaxControllers && effect.controllerIndex > -1, "");

    if (App::m_instance->m_controllers[effect.controllerIndex].haptic)
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
    CRO_ASSERT(effect.controllerIndex < MaxControllers&& effect.controllerIndex > -1, "");

    if (App::m_instance->m_controllers[effect.controllerIndex].haptic)
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

void GameController::rumbleStart(std::int32_t controllerIndex, std::uint16_t strengthLow, std::uint16_t strengthHigh, std::uint32_t duration)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");


    if (controllerIndex > -1 && App::m_instance->m_controllers[controllerIndex].controller)
    {
        if (SDL_GameControllerRumble(App::m_instance->m_controllers[controllerIndex].controller, strengthLow, strengthHigh, duration) != 0)
        {
            auto* error = SDL_GetError();
            LogE << error << " on controller " << controllerIndex << ": rumbleStart()" << std::endl;
        }
    }
}

void GameController::rumbleStop(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    if (controllerIndex > -1 && App::m_instance->m_controllers[controllerIndex].controller)
    {
        if (SDL_GameControllerRumble(App::m_instance->m_controllers[controllerIndex].controller, 0, 0, 0) != 0)
        {
            auto* error = SDL_GetError();
            LogE << error << " on controller " << controllerIndex << ": rumbleEnd()" << std::endl;
        }
    }
}

std::string GameController::getName(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    if (App::m_instance->m_controllers[controllerIndex].controller)
    {
        auto prodID = SDL_GameControllerGetProduct(App::m_instance->m_controllers[controllerIndex].controller);
        return std::string(SDL_GameControllerName(App::m_instance->m_controllers[controllerIndex].controller)) + ", Vendor: " + std::to_string(prodID);
    }
    return "Unknown Device";
}

const cro::String& GameController::getPrintableName(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    if (App::m_instance->m_controllers[controllerIndex].controller)
    {
        return App::m_instance->m_controllers[controllerIndex].printableName;
    }
    static const cro::String fallback("No Controller");
    return fallback;
}

std::int32_t GameController::getControllerCount()
{
    CRO_ASSERT(App::m_instance, "No app running");
    return App::m_instance->m_controllerCount;
}

bool GameController::hasPSLayout(std::int32_t controllerIndex)
{
    CRO_ASSERT(App::m_instance, "No app running");
    CRO_ASSERT(controllerIndex < MaxControllers, "");

    if (controllerIndex < 0)
    {
        return false;
    }

    if (App::m_instance->m_controllers[controllerIndex].controller)
    {
        return App::m_instance->m_controllers[controllerIndex].psLayout;
    }
    return false;
}

void GameController::setLEDColour(std::int32_t controllerIndex, cro::Colour colour)
{
    if (auto* controller = SDL_GameControllerFromInstanceID(controllerIndex); controller)
    {
        SDL_GameControllerSetLED(controller, colour.getRedByte(), colour.getGreenByte(), colour.getBlueByte());
    }
}

bool GameController::applyDSTriggerEffect(std::int32_t controllerIndex, std::int32_t triggers, const DSEffect& settings)
{
    struct DataPacket final
    {
        std::array<std::uint8_t, 2> header = { 0x0c, 0x40 };
        std::array<std::uint8_t, 68> data = {};

        DataPacket() { std::fill(data.begin(), data.end(), 0); }
    };

    if (auto* controller = SDL_GameControllerFromInstanceID(controllerIndex); controller)
    {
        if (SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_PS5)
        {
            DataPacket dataPacket;

            if (triggers & DSTriggerRight)
            {
                std::memcpy(&dataPacket.data[8], &settings, sizeof(DSEffect));
                //dataPacket.data[17] = settings.actuationFrequency;
            }
            if (triggers & DSTriggerLeft)
            {
                std::memcpy(&dataPacket.data[19], &settings, sizeof(DSEffect));
                //dataPacket.data[27] = settings.actuationFrequency;
            }
            return SDL_GameControllerSendEffect(controller, &dataPacket, sizeof(DataPacket)) == 0;
        }
        return false;
    }
    return false;
}

std::uint64_t GameController::getSteamHandle(std::int32_t controllerID)
{
#if SDL_MINOR_VERSION < 30
            return 0;
#else
    if (controllerID < 0 || controllerID >= MaxControllers)
    {
        return 0;
    }
    
    if (auto* c = App::m_instance->m_controllers[controllerID].controller; c != nullptr)
    {
            return SDL_GameControllerGetSteamHandle(c);
    }
    return 0;
#endif
}

//factory functions for DSEffect - based on https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db (MIT)
GameController::DSEffect GameController::DSEffect::createFeedback(std::uint8_t position, std::uint8_t strength)
{
    position = std::min(std::uint8_t(9), position);
    strength = std::min(std::uint8_t(8), strength);

    DSEffect retVal;
    if (strength == 0)
    {
        //defaults to switching the effect off
        return retVal;
    }

    retVal.mode = DSModeFeedback;

    std::uint8_t forceValue = std::uint8_t((strength - 1) & 0x07);
    std::uint32_t forceZones = 0u;
    std::uint16_t activeZones = 0u;

    for (auto i = position; i < 10u; i++)
    {
        forceZones |= std::uint32_t(forceValue << (3 * i));
        activeZones |= std::uint16_t(1 << i);
    }

    retVal.params[0] = std::uint8_t((activeZones >> 0) & 0xff);
    retVal.params[1] = std::uint8_t((activeZones >> 8) & 0xff);
    retVal.params[2] = std::uint8_t((forceZones >> 0) & 0xff);
    retVal.params[3] = std::uint8_t((forceZones >> 8) & 0xff);
    retVal.params[4] = std::uint8_t((forceZones >> 16) & 0xff);
    retVal.params[5] = std::uint8_t((forceZones >> 24) & 0xff);

    return retVal;
}

GameController::DSEffect GameController::DSEffect::createWeapon(std::uint8_t startPosition, std::uint8_t endPosition, std::uint8_t strength)
{
    startPosition = std::clamp(startPosition, std::uint8_t(2), std::uint8_t(7));
    endPosition = std::clamp(endPosition, std::uint8_t(startPosition + 1), std::uint8_t(8));
    strength = std::min(strength, std::uint8_t(8));

    DSEffect retVal;
    if (strength == 0)
    {
        return retVal;
    }

    retVal.mode = DSModeWeapon;
    
    std::uint16_t startAndStopZones = std::uint16_t((1 << startPosition) | (1 << endPosition));

    retVal.params[0] = std::uint8_t((startAndStopZones >> 0) & 0xff);
    retVal.params[1] = std::uint8_t((startAndStopZones >> 8) & 0xff);
    retVal.params[2] = std::uint8_t (strength - 1);

    return retVal;
}

GameController::DSEffect GameController::DSEffect::createVibration(std::uint8_t position, std::uint8_t strength, std::uint8_t frequency)
{
    position = std::clamp(position,std::uint8_t(0), std::uint8_t(9));
    strength = std::clamp(strength, std::uint8_t(0), std::uint8_t(8));

    DSEffect retVal;
    if (strength == 0
        || frequency == 0)
    {
        return retVal;
    }

    retVal.mode = DSModeVibrate;

    std::uint8_t strengthValue = std::uint8_t((strength - 1) & 0x07);
    std::uint32_t amplitudeZones = 0u;
    std::uint16_t activeZones = 0u;

    for (auto i = position; i < 10u; i++)
    {
        amplitudeZones |= std::uint32_t(strengthValue << (3 * i));
        activeZones |= std::uint32_t (1 << i);
    }

    

    retVal.params[0] = std::uint8_t((activeZones >> 0) & 0xff);
    retVal.params[1] = std::uint8_t((activeZones >> 8) & 0xff);
    retVal.params[2] = std::uint8_t((amplitudeZones >> 0) & 0xff);
    retVal.params[3] = std::uint8_t((amplitudeZones >> 8) & 0xff);
    retVal.params[4] = std::uint8_t((amplitudeZones >> 16) & 0xff);
    retVal.params[5] = std::uint8_t((amplitudeZones >> 24) & 0xff);
    //retVal.params[6] = 0;
    //retVal.params[7] = 0;
    retVal.params[8] = frequency;


    return retVal;
}

GameController::DSEffect GameController::DSEffect::createMultiFeedback(const std::array<std::uint8_t, 10u>& values)
{
    DSEffect retVal;
    if (std::all_of(values.begin(), values.end(), [](std::uint8_t i) {return i == 0; }))
    {
        return retVal;
    }

    retVal.mode = DSModeFeedback;

    std::uint32_t forceZones = 0u;
    std::uint32_t activeZones = 0u;

    for (auto i = 0u; i < values.size(); i++)
    {
        if (values[i] != 0)
        {
            std::uint8_t forceValue = std::uint8_t((values[i] - 1) & 0x07);
            forceZones |= std::uint32_t(forceValue << (3 * i));
            activeZones |= std::uint32_t(1 << i);
        }
    }

    retVal.params[0] = std::uint8_t((activeZones >> 0) & 0xff);
    retVal.params[1] = std::uint8_t((activeZones >> 8) & 0xff);
    retVal.params[2] = std::uint8_t((forceZones >> 0) & 0xff);
    retVal.params[3] = std::uint8_t((forceZones >> 8) & 0xff);
    retVal.params[4] = std::uint8_t((forceZones >> 16) & 0xff);
    retVal.params[5] = std::uint8_t((forceZones >> 24) & 0xff);

    return retVal;
}

GameController::DSEffect GameController::DSEffect::createSlopeFeedback(std::uint8_t startPosition, std::uint8_t endPosition, std::uint8_t startStrength, std::uint8_t endStrength)
{
    startPosition = std::clamp(startPosition, std::uint8_t(0), std::uint8_t(8));
    endPosition = std::clamp(endPosition, std::uint8_t(startPosition+1), std::uint8_t(9));
    startStrength = std::clamp(startStrength, std::uint8_t(1), std::uint8_t(8));
    endStrength = std::clamp(endStrength, std::uint8_t(1), std::uint8_t(8));

    std::array<std::uint8_t, 10u> values = {};
    float slope = 1.f * static_cast<float>(endStrength - startStrength) / (endPosition - startPosition);
    for (auto i = startPosition; i < values.size(); i++)
    {
        if (i <= endPosition)
        {
            values[i] = static_cast<std::uint8_t>(std::round(static_cast<float>(startStrength) + (slope * (i - startPosition))));
        }
        else
        {
            values[i] = endStrength;
        }
    }


    return createMultiFeedback(values);
}

GameController::DSEffect GameController::DSEffect::createMultiVibration(const std::array<std::uint8_t, 10u>& values, std::uint8_t frequency)
{
    DSEffect retVal;
    if (frequency == 0
        || std::all_of(values.begin(), values.end(), [](std::uint8_t i) {return i == 0; }))
    {
        return retVal;
    }

    retVal.mode = DSModeVibrate;

    std::uint32_t strengthZones = 0;
    std::uint16_t activeZones = 0;

    for (auto i = 0u; i < values.size(); i++)
    {
        if (values[i] != 0)
        {
            std::uint8_t strengthValue = std::uint8_t((values[i] - 1) & 0x07);
            strengthZones |= std::uint32_t(strengthValue << (3 * i));
            activeZones |= std::uint16_t(1 << i);
        }
    }


    retVal.params[0] = std::uint8_t((activeZones >> 0) & 0xff);
    retVal.params[1] = std::uint8_t((activeZones >> 8) & 0xff);
    retVal.params[2] = std::uint8_t((strengthZones >> 0) & 0xff);
    retVal.params[3] = std::uint8_t((strengthZones >> 8) & 0xff);
    retVal.params[4] = std::uint8_t((strengthZones >> 16) & 0xff);
    retVal.params[5] = std::uint8_t((strengthZones >> 24) & 0xff);
    //retVal.params[6] = 0;
    //retVal.params[7] = 0;
    retVal.params[8] = frequency;

    return retVal;
}