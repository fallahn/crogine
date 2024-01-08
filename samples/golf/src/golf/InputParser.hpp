/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "InputBinding.hpp"
#include "Swingput.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/glm/vec3.hpp>

namespace cro
{
    class Scene;
}

struct SharedStateData;
class InputParser final
{
public:
       
    InputParser(const SharedStateData&, cro::Scene*);

    void handleEvent(const cro::Event&);
    void setHoleDirection(glm::vec3);
    void setClub(float); //picks closest club to given distance
    float getYaw() const; //yaw in world space (includes facing direction)
    float getRotation() const; //relative rotation
    float getRotationSpeed() const;
    float getDirection() const { return m_holeDirection; }
    float getCamMotion() const { return m_camMotion; }
    float getCamRotation() const;
    bool getButtonState(std::int32_t binding) const;

    float getPower() const; //0-1 multiplied by selected club
    float getHook() const; //-1 to -1 * some angle, club defined

    std::int32_t getClub() const;

    // *sigh* be careful with this, the int param can be implicitly converted to bool...
    void setActive(bool active, std::int32_t terrain, bool isCPU = false, std::uint8_t lie = 1);
    void setSuspended(bool);
    void setEnableFlags(std::uint16_t); //bits which are set are *enabled*
    void setMaxClub(float, bool atTee); //based on overall distance of hole
    void setMaxClub(std::int32_t); //force set when only wanting wedges for example
    void resetPower();
    void update(float);

    bool inProgress() const;
    bool getActive() const;
    bool isAiming() const { return m_state == InputParser::State::Aim; }

    bool isSwingputActive() const { return m_swingput.isActive() && m_state != State::Drone; }
    float getSwingputPosition() const { return m_swingput.getActivePoint().y; }
    void setMouseScale(float scale) { CRO_ASSERT(scale > 0, ""); m_swingput.setMouseScale(scale); }

    void setMaxRotation(float);
    float getMaxRotation() const { return m_maxRotation; }

    glm::vec2 getSpin() const { return m_spin; }
    bool isSpinputActive() const { return (m_inputFlags & InputFlag::SpinMenu) != 0; }

    const InputBinding getInputBinding() const { return m_inputBinding; }

    struct StrokeResult final
    {
        glm::vec3 impulse = glm::vec3(0.f);
        glm::vec2 spin = glm::vec2(0.f);
        float hook = 0.f;
    };
    StrokeResult getStroke(std::int32_t club, std::int32_t facing, float holeDistance) const; //facing is -1 or 1 to decide on slice/hook

    float getEstimatedDistance() const; //projected magnitude of distance vector of the current club with the current spin setting

    static constexpr std::uint32_t CPU_ID = 1337u;

    void doFastStroke(float hook, float power); //performed by 'fast' CPU


private:
    const SharedStateData& m_sharedData;
    const InputBinding& m_inputBinding;
    cro::Scene* m_gameScene;

    Swingput m_swingput;

    std::uint16_t m_inputFlags;
    std::uint16_t m_prevFlags;
    std::uint16_t m_enableFlags;
    std::uint16_t m_prevDisabledFlags; //< for raising events on disabled inputs
    std::uint16_t m_prevStick;
    float m_analogueAmount;
    float m_inputAcceleration;
    float m_camMotion;

    std::int32_t m_mouseWheel;
    std::int32_t m_prevMouseWheel;
    //std::int32_t m_mouseMove;
    //std::int32_t m_prevMouseMove;

    bool m_isCPU;
    cro::Clock m_doubleTapClock; //prevent accidentally double tapping action

    float m_holeDirection; //radians
    float m_rotation; //+- max rads
    float m_maxRotation;

    float m_power;
    float m_hook;
    float m_powerbarDirection;
    glm::vec2 m_spin;

    bool m_active;
    bool m_suspended;

    enum class State
    {
        Aim, Power, Stroke,
        Flight, Drone
    }m_state;

    std::int32_t m_currentClub;
    std::int32_t m_firstClub;
    std::int32_t m_clubOffset; //offset ID from first club

    std::vector<float> m_bunkerWavetable;
    std::vector<float> m_roughWavetable;
    std::size_t m_bunkerTableIndex;
    std::size_t m_roughTableIndex;

    std::int32_t m_terrain;
    std::uint8_t m_lie;
    float m_estimatedDistance;
    void updateDistanceEstimation();

    void updateStroke(float);
    void updateDroneCam(float);
    void updateSpin(float);

    void rotate(float);
    void checkControllerInput();
    void checkMouseInput();
    glm::vec2 getRotationalInput(std::int32_t xAxis, std::int32_t yAxis) const; //used for drone cam and spin amount

    cro::Clock m_iconTimer;
    bool m_iconActive;
    void beginIcon();
    void endIcon();
};