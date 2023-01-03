/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include <crogine/detail/Types.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/core/HiResTimer.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <array>

struct SharedStateData;
constexpr float MaxSwingputDistance = 200.f;
class Swingput final : public cro::GuiClient
{
public:
    explicit Swingput(const SharedStateData&);

    //returns true if this consumed the event
    bool handleEvent(const cro::Event&);

    //returns true if the shot was taken and result
    //is ready in power/hook
    bool process(float);

    bool isActive() const { return m_state != State::Inactive; }

    //set to -1 to deactivate, else current player ID
    void setEnabled(std::int32_t enabled);
    std::int32_t getEnabled() const { return m_enabled; }

    float getPower() const { return m_power; }
    float getHook() const { return m_hook; }

    glm::vec2 getBackPoint() const { return m_backPoint; }
    glm::vec2 getActivePoint() const { return m_activePoint; }
    glm::vec2 getFrontPoint() const { return m_frontPoint; }

    struct State final
    {
        enum
        {
            Inactive,
            Swing,
            Summarise,

            Count
        };
    };
    std::int32_t getState() const { return m_state; }

    void setMouseScale(float scale) { m_mouseScale = scale; }

private:
    const SharedStateData& m_sharedData;
    std::int32_t m_enabled;

    glm::vec2 m_backPoint;
    glm::vec2 m_activePoint;
    glm::vec2 m_frontPoint;

    float m_power;
    float m_hook;

    float m_maxVelocity; //depends on controller or mouse input
    float m_mouseScale;

    std::int16_t m_lastLT;
    std::int16_t m_lastRT;

    std::int32_t m_state = State::Inactive;
    const std::array<std::string, State::Count> StateStrings =
    {
        "Inactive", "Swing", "Summarise"
    };

    cro::HiResTimer m_timer;
    float m_elapsedTime;
};