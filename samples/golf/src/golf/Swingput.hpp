/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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
    bool handleEvent(const cro::Event&, std::uint16_t& flags, std::int32_t state);
    bool isActive() const { return m_state != State::Inactive; }

    //checks to see if an active swing was cancelled by letting go of
    //the thumbstick must be called by input parser at top of processing
    void assertIdled(float, std::uint16_t& flags, std::int32_t state);

    //processes mouse swing if it's active and returns true if swing was taken
    bool processMouseSwing();
    float getMousePower() const { return m_mouseSwing.power; }
    float getMouseHook() const { return m_mouseSwing.hook; }
    void setMouseScale(float s) { m_mouseSwing.scale = s; }

    //set to -1 to deactivate, else current player ID
    void setEnabled(std::int32_t enabled);
    std::int32_t getEnabled() const { return m_enabled; }

    float getHook() const { return m_hook; }
    float getGaugePosition() const { return m_gaugePosition; }

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

private:
    const SharedStateData& m_sharedData;
    std::int32_t m_enabled;

    cro::HiResTimer m_tempoTimer;

    float m_hook;
    float m_gaugePosition;

    std::int16_t m_lastLT;
    std::int16_t m_lastRT;
    std::int16_t m_strokeStartPosition;

    //used to track if we should cancel from letting go of the stick
    std::int32_t m_activeStick;
    std::int16_t m_lastAxisposition;
    float m_cancelTimer;
    bool m_inCancelZone;

    std::int32_t m_state = State::Inactive;
    const std::array<std::string, State::Count> StateStrings =
    {
        "Inactive", "Swing", "Summarise"
    };

    struct MouseSwing final
    {
        glm::vec2 backPoint = glm::vec2(0.f);
        glm::vec2 activePoint = glm::vec2(0.f);
        glm::vec2 frontPoint = glm::vec2(0.f);

        float power = 0.f;
        float hook = 0.f;
        float scale = 1.f;

        cro::HiResTimer timer;
        float elapsedTime = 0.f;

        bool active = false;

        static constexpr float MaxVelocity = 1700.f;
        static constexpr float MaxAccuracy = 20.f;

        void startStroke()
        {
            backPoint = glm::vec2(0.f);
            activePoint = glm::vec2(0.f);
            frontPoint = glm::vec2(0.f);

            power = 0.f;
            hook = 0.f;

            active = true;
        }
        void endStroke()
        {
            activePoint = glm::vec2(0.f);
            active = false;
        }
    }m_mouseSwing;

    void setGaugeFromController(std::int16_t);
};