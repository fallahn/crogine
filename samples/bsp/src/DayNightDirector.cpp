/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "DayNightDirector.hpp"
#include "CommandIDs.hpp"
#include "GameConsts.hpp"

#include <crogine/detail/glm/gtx/norm.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/core/Console.hpp>

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/ecs/Sunlight.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>


namespace
{
    //sky colours
    /*
    0.f    Dark Blue
    0.05f  Orange/Yellow (sunrise)
    0.08f  White (full day)
    0.42f  White (full day)
    0.45f  Orange/Red (sunset)
    0.5f   Dark Blue
    0.55f  Blue
    0.75f  LightBlue (midnight)
    0.95f  Blue
    */

    struct SkyColour final
    {
        SkyColour(float i, glm::vec4 v) : interpPosition(i), value(v) {}
        float interpPosition = 0.f;
        glm::vec4 value = glm::vec4(1.f);
    };

    //these are in HSV space - note S and V are normalised
    std::array SkyColours =
    {
        SkyColour(0.f, glm::vec4(217.f, 0.91f, 0.18f, 1.f)), //
        //SkyColour(0.05f, glm::vec4(415.f, 1.f, 0.78f, 1.f)), //
        SkyColour(0.08f, glm::vec4(217.f, 0.f, 1.f, 1.f)), //
        SkyColour(0.42f, glm::vec4(217.f, 0.f, 1.f, 1.f)), //
        //SkyColour(0.45f, glm::vec4(23.f, 1.f, 1.f, 1.f)),
        SkyColour(0.55f, glm::vec4(217.f, 0.91f, 0.18f, 1.f)), //
        SkyColour(0.65f, glm::vec4(217.f, 0.78f, 0.4f, 1.f)), // blue
        SkyColour(0.75f, glm::vec4(234.f, 0.81f, 0.37f, 1.f)), // lightblur
        SkyColour(0.85f, glm::vec4(217.f, 0.78f, 0.4f, 1.f)), //blue
        SkyColour(1.f, glm::vec4(217.f, 0.91f, 0.18f, 1.f)) //repeat first val for wrapping
    };

    glm::vec4 lerp(glm::vec4 a, glm::vec4 b, float t)
    {
        return { a.r * (1.f - t) + b.r * t,
                a.g * (1.f - t) + b.g * t,
                a.b * (1.f - t) + b.b * t,
                1.f };
    }

    //note that this expect the S and V values to be normalised.
    glm::vec4 HSVtoRGB(glm::vec4 input)
    {
        if (input.g <= 0)
        {
            //no saturation, ie black and white
            return { input.b, input.b, input.b, 1.f };
        }

        float hh = input.r;
        if (hh >= 360.f)
        {
            hh -= 360.f;
        }
        else if (hh < 0)
        {
            hh += 360.f;
        }
        hh /= 60.f;

        int i = static_cast<int>(hh);
        float ff = hh - i;

        float p = input.b * (1.f - input.g);
        float q = input.b * (1.f - (input.g * ff));
        float t = input.b * (1.f - (input.g * (1.f - ff)));

        switch (i)
        {
        case 0:
            return { input.b, t, p , 1.f };
        case 1:
            return { q, input.b, p, 1.f };
        case 2:
            return { p, input.b, t, 1.f };
        case 3:
            return { p, q, input.b, 1.f };
        case 4:
            return { t, p , input.b, 1.f };
        case 5:
        default:
            return { input.b, p, q, 1.f };
        }
        return {};
    }

    constexpr std::uint32_t DayMinutes = 24 * 60;
    constexpr float RadsPerMinute = cro::Util::Const::TAU;// / 6.f; //6 minutes per cycle
    constexpr float RadsPerSecond = RadsPerMinute / 60.f;

    constexpr float CorrectionSpeed = 50.f;
}

DayNightDirector::DayNightDirector()
    : m_timeOfDay       (0.f),
    m_targetTime        (0.f),
    m_correctTime       (false),
    m_cycleSpeed        (1.f),
    m_currentSkyIndex   (0),
    m_nextSkyIndex      (1)
{
    registerCommand("tod", 
        [&](const std::string& param)
        {
            try
            {
                auto minutes = std::stoi(param);
                setTimeOfDay(minutes);

                cro::Console::print("Set time of day to " + param);
            }
            catch (...)
            {
                cro::Console::print(param + ": Invalid value. Must be in minutes.");
            }
        });
}

//public
void DayNightDirector::handleMessage(const cro::Message&)
{

}

void DayNightDirector::process(float dt)
{
    //check if we need to update sky interp
    if (m_timeOfDay < SkyColours[m_currentSkyIndex].interpPosition
        || m_timeOfDay > SkyColours[m_nextSkyIndex].interpPosition)
    {
        m_currentSkyIndex = (m_currentSkyIndex + 1) % SkyColours.size();
        m_nextSkyIndex = (m_nextSkyIndex + 1) % SkyColours.size();

        //we've looped (is there a more elegant way to handle this?)
        if (m_nextSkyIndex < m_currentSkyIndex)
        {
            m_currentSkyIndex = 0;
            m_nextSkyIndex = 1;
        }
    }

    //interpolate from colour range - do this before incrementing time
    const auto& current = SkyColours[m_currentSkyIndex];
    const auto& next = SkyColours[m_nextSkyIndex];
    
    const float position = (m_timeOfDay - current.interpPosition);
    const float span = (next.interpPosition - current.interpPosition);
    const float interpAmount = std::min(1.f, std::max(0.f, position / span));

    auto val = lerp(current.value, next.value, interpAmount);
    auto col = HSVtoRGB(val);
    getScene().getSunlight().getComponent<cro::Sunlight>().setColour(cro::Colour(col));


    //used to check if we switched from day/night
    bool dayTime = (m_timeOfDay < 0.5f);

    //now increment the time
    float multiplier = 1.f;
    if (m_correctTime)
    {
        float diff = m_targetTime - m_timeOfDay;

        multiplier = diff * CorrectionSpeed;

        if (std::abs(diff) < 0.01f)
        {
            m_correctTime = false;
        }
    }
    multiplier *= m_cycleSpeed;

    m_timeOfDay = std::fmod(m_timeOfDay + ((RadsPerSecond * dt * multiplier) / cro::Util::Const::TAU), 1.f);


    //update the shadow map projector when flipping day / night
    auto& target = getScene().getSunlight().getComponent<TargetTransform>();
    if (dayTime)
    {
        if (m_timeOfDay >= 0.5f)
        {
            //switch to night
            target.startPosition = { 0.f, 0.f, SeaRadius };
            target.startRotation = glm::quat(1.f, 0.f, 0.f, 0.f);

            target.endPosition = { 0.f, 0.f, -SeaRadius };
            target.endRotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), cro::Util::Const::PI - 0.001f, cro::Transform::X_AXIS);

            target.interpolation = 0.f;
            target.active = true;
        }
    }
    else
    {
        if (m_timeOfDay < 0.5f)
        {
            //switch to day
            target.startPosition = { 0.f, 0.f, -SeaRadius };
            target.startRotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), cro::Util::Const::PI + 0.001f, cro::Transform::X_AXIS);

            target.endPosition = { 0.f, 0.f, SeaRadius };
            target.endRotation = glm::quat(1.f, 0.f, 0.f, 0.f);

            target.interpolation = 0.f;
            target.active = true;
        }
    }

    //interp position of shadow caster if mid switch
    if (target.active)
    {
        auto diff = target.endPosition - target.startPosition;
        auto& tx = getScene().getSunlight().getComponent<cro::Transform>();

        float len2 = glm::length2(diff);
        if (len2 > 0.2f)
        {
            target.interpolation = std::min(1.f, target.interpolation + (dt * 2.f));

            auto pos = target.startPosition + (diff * target.interpolation);
            auto rot = glm::slerp(target.startRotation, target.endRotation, target.interpolation);

            tx.setPosition(pos);
            tx.setRotation(rot);
        }
        else
        {
            tx.setPosition(target.endPosition);
            tx.setRotation(target.endRotation);

            target.active = false;
        }
    }



    //set sun/moon rotation
    cro::Command cmd;
    cmd.targetFlags = CommandID::Game::SunMoonNode;
    cmd.action = [&](cro::Entity e, float)
    {
        auto& tx = e.getComponent<cro::Transform>();
        tx.setRotation(-cro::Transform::X_AXIS, cro::Util::Const::TAU * m_timeOfDay);
        tx.rotate(cro::Transform::Y_AXIS, -cro::Util::Const::PI / 8.f);
    };
    sendCommand(cmd);
}

void DayNightDirector::setTimeOfDay(std::uint32_t minutes)
{
    //entering 0 you would expect midnight but in our case
    //you get 6am so we need to add 18 hours
    const std::uint32_t offset = 18 * 60;
    minutes += offset;

    minutes = minutes % DayMinutes;
    m_targetTime = static_cast<float>(minutes) / DayMinutes;
    m_correctTime = true;
}

void DayNightDirector::setCycleSpeed(float speed)
{
    m_cycleSpeed = std::min(1.f, speed);
}