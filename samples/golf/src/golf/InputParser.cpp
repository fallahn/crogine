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

#include "InputParser.hpp"
#include "InputBinding.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "Terrain.hpp"
#include "SharedStateData.hpp"
#include "GameConsts.hpp"
#include "CameraFollowSystem.hpp"
#include "CommandIDs.hpp"
#include "BallSystem.hpp"
#include "PacketIDs.hpp"

#include <AchievementStrings.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Wavetable.hpp>

using namespace cl;

namespace
{
    constexpr float SpeedReduction = 1.4f; //bar speed is divided by this when putt assist is enabled
    constexpr float SideSpinReduction = 0.3f; //power is reduced p * (1.f - (SideSpinReduction * abs(sideSpin)))

    static constexpr float RotationSpeed = 1.2f;
    static constexpr float MaxRotation = 0.36f;

    static constexpr float MinPower = 0.01f;
    static constexpr float MaxPower = 1.f - MinPower;

    static constexpr float MinAcceleration = 0.5f;
    static constexpr float MinBarSpeed = 0.9f; //base speed of power bar up to level 10

    const cro::Time DoubleTapTime = cro::milliseconds(200);
}

InputParser::InputParser(const SharedStateData& sd, cro::Scene* s)
    : m_sharedData      (sd),
    m_inputBinding      (sd.inputBinding),
    m_gameScene         (s),
    m_swingput          (sd),
    m_inputFlags        (0),
    m_prevFlags         (0),
    m_enableFlags       (std::numeric_limits<std::uint16_t>::max()),
    m_prevDisabledFlags (0),
    m_prevStick         (0),
    m_analogueAmount    (0.f),
    m_inputAcceleration (0.f),
    m_camMotion         (0.f),
    m_mouseWheel        (0),
    m_prevMouseWheel    (0),
    //m_mouseMove         (0),
    //m_prevMouseMove     (0),
    m_isCPU             (false),
    m_holeDirection     (0.f),
    m_rotation          (0.f),
    m_maxRotation       (MaxRotation),
    m_power             (0.f),
    m_hook              (0.5f),
    m_powerbarDirection (1.f),
    m_spin              (0.f),
    m_active            (false),
    m_suspended         (false),
    m_state             (State::Aim),
    m_currentClub       (ClubID::Driver),
    m_firstClub         (ClubID::Driver),
    m_clubOffset        (0),
    m_bunkerTableIndex  (0),
    m_roughTableIndex   (0),
    m_terrain           (TerrainID::Fairway),
    m_lie               (1),
    m_estimatedDistance (0.f),
    m_iconActive        (false)
{
    m_bunkerWavetable = cro::Util::Wavetable::sine(0.25f, 0.035f);
    m_roughWavetable = cro::Util::Wavetable::sine(0.25f, 0.025f);
}

//public
void InputParser::handleEvent(const cro::Event& evt)
{
    const auto toggleDroneCam =
        [&]()
    {
        if (m_gameScene != nullptr //we don't do this on the driving range
            && (m_inputFlags & InputFlag::SpinMenu) == 0) 
        {
            if (m_state == State::Aim)
            {
                m_state = State::Drone;
                auto* msg = cro::App::postMessage<SceneEvent>(MessageID::SceneMessage);
                msg->type = SceneEvent::RequestSwitchCamera;
                msg->data = CameraID::Drone;

                Achievements::awardAchievement(AchievementStrings[AchievementID::BirdsEyeView]);
            }
            else if (m_state == State::Drone)
            {
                m_state = State::Aim;
                auto* msg = cro::App::postMessage<SceneEvent>(MessageID::SceneMessage);
                msg->type = SceneEvent::RequestSwitchCamera;
                msg->data = CameraID::Player;
            }
        }
    };

    if (m_active &&
        !m_swingput.handleEvent(evt))
    {
        //apply to input mask
        if (evt.type == SDL_KEYDOWN
            && evt.key.repeat == 0)
        {
            if (m_isCPU && evt.key.windowID != CPU_ID)
            {
                return;
            }

            if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
            {
                m_inputFlags |= InputFlag::Up;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
            {
                m_inputFlags |= InputFlag::Left;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
            {
                m_inputFlags |= InputFlag::Right;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
            {
                m_inputFlags |= InputFlag::Down;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
            {
                m_inputFlags |= InputFlag::Action;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
            {
                m_inputFlags |= InputFlag::NextClub;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
            {
                m_inputFlags |= InputFlag::PrevClub;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::CancelShot])
            {
                m_inputFlags |= InputFlag::Cancel;
                //cro::App::getWindow().setMouseCaptured(!m_isCPU);
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::SpinMenu])
            {
                if (m_state != State::Drone)
                {
                    m_inputFlags |= InputFlag::SpinMenu;
                    //cro::App::getWindow().setMouseCaptured(!m_isCPU);
                }
            }
            else if (evt.key.keysym.sym == SDLK_1)
            {
                toggleDroneCam();
            }
        }
        else if (evt.type == SDL_KEYUP)
        {
            if (m_isCPU && evt.key.windowID != CPU_ID)
            {
                return;
            }

            if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
            {
                m_inputFlags &= ~InputFlag::Up;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
            {
                m_inputFlags &= ~InputFlag::Left;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
            {
                m_inputFlags &= ~InputFlag::Right;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
            {
                m_inputFlags &= ~InputFlag::Down;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
            {
                m_inputFlags &= ~InputFlag::Action;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
            {
                m_inputFlags &= ~InputFlag::NextClub;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
            {
                m_inputFlags &= ~InputFlag::PrevClub;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::CancelShot])
            {
                m_inputFlags &= ~InputFlag::Cancel;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::SpinMenu])
            {
                m_inputFlags &= ~InputFlag::SpinMenu;
            }
        }
        else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
        {
            auto controllerID = activeControllerID(m_inputBinding.playerID);
            if (!m_isCPU &&
                evt.cbutton.which == cro::GameController::deviceID(controllerID))
            {
                if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
                {
                    m_inputFlags |= InputFlag::Action;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::NextClub])
                {
                    m_inputFlags |= InputFlag::NextClub;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::PrevClub])
                {
                    m_inputFlags |= InputFlag::PrevClub;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CancelShot])
                {
                    m_inputFlags |= InputFlag::Cancel;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::SpinMenu])
                {
                    if (m_state != State::Drone)
                    {
                        m_inputFlags |= InputFlag::SpinMenu;
                    }
                }

                else if (evt.cbutton.button == cro::GameController::DPadLeft)
                {
                    m_inputFlags |= InputFlag::Left;
                }
                else if (evt.cbutton.button == cro::GameController::DPadRight)
                {
                    m_inputFlags |= InputFlag::Right;
                }
                else if (evt.cbutton.button == cro::GameController::DPadUp)
                {
                    m_inputFlags |= InputFlag::Up;
                }
                else if (evt.cbutton.button == cro::GameController::DPadDown)
                {
                    m_inputFlags |= InputFlag::Down;
                }

                else if (evt.cbutton.button == cro::GameController::ButtonRightStick)
                {
                    toggleDroneCam();
                }
            }
        }
        else if (evt.type == SDL_CONTROLLERBUTTONUP)
        {
            auto controllerID = activeControllerID(m_inputBinding.playerID);
            if (!m_isCPU &&
                evt.cbutton.which == cro::GameController::deviceID(controllerID))
            {
                if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
                {
                    m_inputFlags &= ~InputFlag::Action;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::NextClub])
                {
                    m_inputFlags &= ~InputFlag::NextClub;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::PrevClub])
                {
                    m_inputFlags &= ~InputFlag::PrevClub;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CancelShot])
                {
                    m_inputFlags &= ~InputFlag::Cancel;
                }
                else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::SpinMenu])
                {
                    m_inputFlags &= ~InputFlag::SpinMenu;
                }

                else if (evt.cbutton.button == cro::GameController::DPadLeft)
                {
                    m_inputFlags &= ~InputFlag::Left;
                }
                else if (evt.cbutton.button == cro::GameController::DPadRight)
                {
                    m_inputFlags &= ~InputFlag::Right;
                }
                else if (evt.cbutton.button == cro::GameController::DPadUp)
                {
                    m_inputFlags &= ~InputFlag::Up;
                }
                else if (evt.cbutton.button == cro::GameController::DPadDown)
                {
                    m_inputFlags &= ~InputFlag::Down;
                }
            }
        }

        //this would be nice - but I cba to block the input when, say,
        //a menu is open so clicking the menu doesn't take a swing...
        /*else if (evt.type == SDL_MOUSEBUTTONDOWN)
        {
            if (evt.button.button == SDL_BUTTON_LEFT)
            {
                m_inputFlags |= InputFlag::Action;
            }
        }
        else if (evt.type == SDL_MOUSEBUTTONUP)
        {
            if (evt.button.button == SDL_BUTTON_LEFT)
            {
                m_inputFlags &= ~InputFlag::Action;
            }
        }*/

        else if (evt.type == SDL_MOUSEWHEEL)
        {
            m_mouseWheel += evt.wheel.y;
        }
        /*else if (evt.type == SDL_MOUSEMOTION)
        {
            m_mouseMove += evt.motion.xrel;
        }*/

    }
    else
    {
        m_inputFlags = 0;
    }
}

void InputParser::setHoleDirection(glm::vec3 dir)
{
    //note that this might be looking at a target other than the hole.
    if (auto len2 = glm::length2(dir); len2 > 0)
    {
        auto length = std::sqrt(len2);
        auto direction = dir / length;
        m_holeDirection = std::atan2(-direction.z, direction.x);

        m_rotation = 0.f;
    }
}

void InputParser::setClub(float dist)
{
    //assume each club can go a little further than its rating
    m_currentClub = ClubID::SandWedge;
    while ((Clubs[m_currentClub].getTarget(dist) * 1.04f) < dist
        && m_currentClub != m_firstClub)
    {
        auto clubCount = ClubID::Putter - m_firstClub;
        do
        {
            m_clubOffset = (m_clubOffset + (clubCount - 1)) % clubCount;
            m_currentClub = m_firstClub + m_clubOffset;
        } while ((m_inputBinding.clubset & ClubID::Flags[m_currentClub]) == 0
            && m_currentClub != m_firstClub);//prevent inf loop
    }

    auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;

    updateDistanceEstimation();
}

float InputParser::getYaw() const
{
    return m_holeDirection + m_rotation;
}

float InputParser::getRotation() const
{
    return m_rotation;
}

float InputParser::getRotationSpeed() const
{
    return RotationSpeed * m_maxRotation * m_analogueAmount;
}

float InputParser::getCamRotation() const
{
    if (m_active 
        && m_state == State::Aim
        && (m_inputFlags & ~(InputFlag::Up | InputFlag::Down)) == 0) //ignore these as right stick might set them
    {
        if (cro::Keyboard::isKeyPressed(SDLK_4))
        {
            return 1.f;
        }
        if (cro::Keyboard::isKeyPressed(SDLK_5))
        {
            return -1.f;
        }

        auto x = -cro::GameController::getAxisPosition(activeControllerID(m_inputBinding.playerID), cro::GameController::AxisRightX);
        const auto dz = LeftThumbDeadZone / 4;
        if (x < -dz || x > dz)
        {
            return std::pow(static_cast<float>(x) / cro::GameController::AxisMax, 5.f);
        }
    }

    return 0.f;
}

bool InputParser::getButtonState(std::int32_t binding) const
{
    switch (binding)
    {
    default: return false;
    case InputBinding::Action:
        return (m_inputFlags & InputFlag::Action) != 0;
    case InputBinding::Left:
        return (m_inputFlags & InputFlag::Left) != 0;
    case InputBinding::Right:
        return (m_inputFlags & InputFlag::Right) != 0;
        //TODO stop being lazy and implement the rest of these...
    }
}

float InputParser::getPower() const
{
    return MinPower + (MaxPower * cro::Util::Easing::easeInSine(m_power));
}

float InputParser::getHook() const
{
    float hook = m_hook;
    if (m_terrain == TerrainID::Bunker
        && m_lie == 0)
    {
        //TODO include the current player/CPU skill level here?
        hook = std::min(1.f, hook * (1.f + (hook * 0.0004f)));
    }
    
    return hook * 2.f - 1.f;
}

std::int32_t InputParser::getClub() const
{
    return m_currentClub;
}

void InputParser::setActive(bool active, std::int32_t terrain, bool isCPU, std::uint8_t lie)
{
    CRO_ASSERT(terrain < TerrainID::Count, "");
    m_active = active;
    m_terrain = terrain;
    m_isCPU = isCPU;
    m_lie = lie;
    m_state = State::Aim;
    //if the parser was suspended when set active then make sure un-suspending it returns the correct state.
    m_suspended = active;
    if (active)
    {
        resetPower();
        m_inputFlags = 0;
        m_spin = glm::vec2(0.f);

        m_swingput.setEnabled((m_enableFlags == std::numeric_limits<std::uint16_t>::max()) && !isCPU ? m_inputBinding.playerID : -1);

        updateDistanceEstimation();
    }
    else
    {
        m_swingput.setEnabled(-1);
    }
}

void InputParser::setSuspended(bool suspended)
{
    if (suspended)
    {
        m_suspended = m_active;
        m_active = false;
    }
    else
    {
        m_active = m_suspended;
    }
    updateDistanceEstimation();
}

void InputParser::setEnableFlags(std::uint16_t flags)
{
    m_enableFlags = flags;

    m_swingput.setEnabled(flags == std::numeric_limits<std::uint16_t>::max() ? m_swingput.getEnabled() : -1);
}

void InputParser::setMaxClub(float dist, bool atTee)
{
    //a fudge to allow a full set on any hole bigger than pitch n putt
    if (!atTee)
    {
        if (dist > 115.f)
        {
            dist = 1000.f;
        }
    }
    else
    {
        //teeing off we want to allow all the way up to wood
        //on par 3s
        dist = std::max(dist, Clubs[ClubID::FiveWood].getBaseTarget());
        //if (dist < Clubs[ClubID::FourIron].getBaseTarget() - 5.f)
        //{
        //    dist = Clubs[ClubID::FourIron].getBaseTarget() * 1.06f;
        //}
        //else
        //{
        //    dist = 1000.f;
        //}
    }

    m_firstClub = ClubID::SandWedge;

    while ((Clubs[m_firstClub].getBaseTarget()/* * 1.05f*/) < dist
        && m_firstClub != ClubID::Driver)
    {
        //this WILL get stuck in an infinite loop if the clubset is 0 for some reason
        do
        {
            m_firstClub--;
        } while ((m_inputBinding.clubset & ClubID::Flags[m_firstClub]) == 0
            && m_firstClub != ClubID::Driver);
    }

    //this isn't perfect so give one extra club wiggle room
    //if (!atTee)
    {
        m_firstClub = std::max(0, m_firstClub - 1);
    }
    m_currentClub = m_firstClub;
    m_clubOffset = 0;

    auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;

    updateDistanceEstimation();
}

void InputParser::setMaxClub(std::int32_t clubID)
{
    CRO_ASSERT(clubID < ClubID::Putter, "");

    m_firstClub = clubID;
    while ((m_inputBinding.clubset & ClubID::Flags[m_firstClub]) == 0
        && m_firstClub != ClubID::PitchWedge)
    {
        m_firstClub++;
    }

    m_currentClub = m_firstClub;
    m_clubOffset = 0;

    auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;

    updateDistanceEstimation();
}

void InputParser::resetPower()
{
    m_power = 0.f;
    m_hook = 0.5f;
    m_powerbarDirection = 1.f;
}

void InputParser::update(float dt)
{
    if (m_inputFlags & (InputFlag::Left | InputFlag::Right | InputFlag::Up | InputFlag::Down))
    {
        m_inputAcceleration = std::min(1.f, m_inputAcceleration + dt);
    }
    else
    {
        m_inputAcceleration = 0.f;
    }

    if ((m_inputFlags & InputFlag::SpinMenu)
         && (m_enableFlags & InputFlag::SpinMenu))
    {
        updateSpin(dt);
    }
    else if (m_state == State::Drone)
    {
        updateDroneCam(dt);
    }
    else
    {
        //drone controls handle controller independently
        checkControllerInput();
        checkMouseInput();
        updateStroke(dt);
    }

    m_prevFlags = m_inputFlags;

    if (m_state == State::Aim
        || m_state == State::Drone)
    {
        m_bunkerTableIndex = (m_bunkerTableIndex + 1) % m_bunkerWavetable.size();
        m_roughTableIndex = (m_roughTableIndex + 1) % m_roughWavetable.size();
    }

    if (m_iconActive 
        && m_iconTimer.elapsed() > cro::seconds(2.f))
    {
        endIcon();
    }
}

bool InputParser::inProgress() const
{
    return (m_state == State::Power || m_state == State::Stroke);
}

bool InputParser::getActive() const
{
    return m_active;
}

void InputParser::setMaxRotation(float rotation)
{
    m_maxRotation = std::max(0.05f, std::min(cro::Util::Const::PI / 2.f/*MaxRotation*/, rotation));
}

InputParser::StrokeResult InputParser::getStroke(std::int32_t club, std::int32_t facing, float distanceToHole) const
{
    auto pitch = Clubs[club].getAngle();
    auto yaw = getYaw();
    auto power = Clubs[club].getPower(distanceToHole, m_sharedData.imperialMeasurements);

    //add hook/slice to yaw
    auto hook = getHook();
    if (club != ClubID::Putter)
    {
        auto s = cro::Util::Maths::sgn(hook);
        //changing this func changes how accurate a player needs to be
        //sine, quad, cubic, quart, quint in steepness order
        if (Achievements::getActive())
        {
            auto level = Social::getLevel();
            switch (level / 10)
            {
            default:
                hook = cro::Util::Easing::easeOutQuint(hook * s) * s;
                break;
            case 3:
                hook = cro::Util::Easing::easeOutQuart(hook * s) * s;
                break;
            case 2:
                hook = cro::Util::Easing::easeOutCubic(hook * s) * s;
                break;
            case 1:
                hook = cro::Util::Easing::easeOutQuad(hook * s) * s;
                break;
            case 0:
                hook = cro::Util::Easing::easeOutSine(hook * s) * s;
                break;
            }
        }
        else
        {
            hook = cro::Util::Easing::easeOutQuad(hook * s) * s;
        }

        power *= cro::Util::Easing::easeOutSine(getPower());
    }
    else
    {
        power *= getPower();
    }
    yaw += MaxHook * hook;

    float sideSpin = -hook;
    sideSpin *= Clubs[club].getSideSpinMultiplier();

    bool slice = (cro::Util::Maths::sgn(hook) * facing == 1);
    if (!slice)
    {
        //hook causes slightly less spin
        sideSpin *= 0.995f;
    }

    float accuracy = 1.f - std::abs(hook);
    auto spin = getSpin() * accuracy;

    //modulate pitch with topspin
    spin.y *= Clubs[club].getTopSpinMultiplier();
    pitch -= (4.f * cro::Util::Const::degToRad) * spin.y;

    spin.x *= Clubs[club].getSideSpinMultiplier() / 2.f;
    spin.x += sideSpin;

    power *= (1.f - (SideSpinReduction * std::abs(spin.x)));

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    impulse *= power;

    return { impulse, spin, hook };
}

float InputParser::getEstimatedDistance() const
{
    switch (m_terrain)
    {
    default: return m_estimatedDistance;
    case TerrainID::Bunker: return m_estimatedDistance * (1.f - m_bunkerWavetable[m_bunkerTableIndex]);
    case TerrainID::Rough: return m_estimatedDistance * (1.f - m_roughWavetable[m_roughTableIndex]);
    }
    return m_estimatedDistance; 
}

void InputParser::doFastStroke(float accuracy, float power)
{
    //this ONLY works with the CPU because it's been estimating
    //the suppplied power value based on getPower() which has been
    //appplying easeInSine() - and we need to invert that to get the
    //base power value again...
    const auto invSine = [](float x)
    {
        return (std::acos(1.f - x) / cro::Util::Const::PI) * 2.f;
    };

    m_hook = (accuracy + 1.f) / 2.f;
    m_power = invSine(power); //gets eased in on getPower();
    
    m_powerbarDirection = 1.f;
    m_state = State::Flight;

    auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::HitBall;
}

//private
void InputParser::updateDistanceEstimation()
{
    //https://www.iforce2d.net/b2dtut/projected-trajectory

    //really we should be sourcing this the same as getStroke()
    
    auto pitch = Clubs[m_currentClub].getAngle();
    auto power = Clubs[m_currentClub].getPower(0.f, m_sharedData.imperialMeasurements);
    auto spin = getSpin();
    spin.y *= Clubs[m_currentClub].getTopSpinMultiplier();
    pitch -= (4.f * cro::Util::Const::degToRad) * spin.y;

    spin.x *= Clubs[m_currentClub].getSideSpinMultiplier() / 2.f;

    power *= (1.f - (SideSpinReduction * std::abs(spin.x)));

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;
    impulse *= power;

    //multiply the terrain dampening
    impulse *= Dampening[m_terrain] * LieDampening[m_terrain][m_lie];

    static constexpr float dt = 1.f / 60.f; //I'm sure we're redefining this...
    const auto stepVel = impulse * dt;
    constexpr auto stepGrav = BallSystem::Gravity * dt * dt;

    glm::vec3 endPos(1.f);
    //run the estimation until we get a time where the final result has a height < 0 again
    float totalSteps = 1.f;
    do
    {
        endPos = (totalSteps * stepVel) + ((((totalSteps * totalSteps) + totalSteps) * stepGrav) / 2.f);
        totalSteps += 1.f;
    } while (endPos.y > 0.f);

    m_estimatedDistance = glm::length(endPos) * 1.08f; //correction of the average difference of club rating (we're only using this for the range indicator)
}

void InputParser::updateStroke(float dt)
{
    //catch the inputs that where filtered by the
    //enable flags so we can raise their own event for them
    auto disabledFlags = (m_inputFlags & ~m_enableFlags);

    if (m_active)
    {
        if (m_swingput.process(dt))
        {
            //we took our shot
            m_power = m_swingput.getPower();
            m_hook = m_swingput.getHook();

            m_powerbarDirection = 1.f;
            m_state = State::Flight;

            auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::HitBall;
        }

        m_inputFlags &= m_enableFlags;

        switch (m_state)
        {
        default: break;
        case State::Aim:
        {
            //allows the scene to move the putt cam up and down
            m_camMotion = 0.f;
            if (m_inputFlags & InputFlag::Up)
            {
                m_camMotion = 1.f;
            }
            if (m_inputFlags & InputFlag::Down)
            {
                m_camMotion -= 1.f;
            }
            m_camMotion *= m_analogueAmount;


            //rotation
            const float rotation = RotationSpeed * m_maxRotation * m_analogueAmount * dt;

            if (m_inputFlags & InputFlag::Left)
            {
                rotate(rotation);
                beginIcon();
            }

            if (m_inputFlags & InputFlag::Right)
            {
                rotate(-rotation);
                beginIcon();
            }

            if (m_inputFlags & InputFlag::Action)
            {
                m_state = State::Power;
                m_doubleTapClock.restart();
                beginIcon();
            }

            if ((m_prevFlags & InputFlag::PrevClub) == 0
                && (m_inputFlags & InputFlag::PrevClub))
            {
                do
                {
                    auto clubCount = ClubID::Putter - m_firstClub;
                    m_clubOffset = (m_clubOffset + 1) % clubCount;
                    m_currentClub = m_firstClub + m_clubOffset;
                } while ((m_inputBinding.clubset & ClubID::Flags[m_currentClub]) == 0);

                auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::ClubChanged;
                msg->score = m_isCPU ? 0 : 1; //tag this with a value so we know the input triggered this and should play a sound.

                updateDistanceEstimation();
                beginIcon();
            }

            if ((m_prevFlags & InputFlag::NextClub) == 0
                && (m_inputFlags & InputFlag::NextClub))
            {
                do
                {
                    auto clubCount = ClubID::Putter - m_firstClub;
                    m_clubOffset = (m_clubOffset + (clubCount - 1)) % clubCount;
                    m_currentClub = m_firstClub + m_clubOffset;
                } while ((m_inputBinding.clubset & ClubID::Flags[m_currentClub]) == 0);

                auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::ClubChanged;
                msg->score = m_isCPU? 0 : 1;

                updateDistanceEstimation();
                beginIcon();
            }
        }
        break;
        case State::Power:
        {
            if (((m_inputFlags & InputFlag::Cancel) && ((m_prevFlags & InputFlag::Cancel) == 0))
                || (m_power == 0 && m_powerbarDirection < 0))
            {
                m_state = State::Aim;
                resetPower();
                break;
            }

            //move level to 1 and back (returning to 0 is a fluff)
            float speed = dt * 0.7f;
            
            if (m_terrain == TerrainID::Green
                && m_sharedData.showPuttingPower)
            {
                //speed /= SpeedReduction;
                speed *= 0.96f;
            }
            else
            {
                //move more slowly for the first 10 levels
                float increase = std::min(1.f, static_cast<float>(Social::getLevel()) / 10.f);
                speed = (speed * MinBarSpeed) + ((speed * (1.f - MinBarSpeed)) * increase);
            }

            m_power = std::min(1.f, std::max(0.f, m_power + (speed * m_powerbarDirection)));

            if (m_power == 1)
            {
                m_powerbarDirection = -1.f;
            }

            if (m_sharedData.pressHold)
            {
                if ((m_inputFlags & InputFlag::Action) == 0 && (m_prevFlags & InputFlag::Action))
                {
                    m_powerbarDirection = 1.f;

                    m_state = State::Stroke;
                    m_doubleTapClock.restart();
                    beginIcon();
                }
            }
            else
            {
                if ((m_inputFlags & InputFlag::Action) && ((m_prevFlags & InputFlag::Action) == 0))
                {
                    if (m_doubleTapClock.elapsed() > DoubleTapTime)
                    {
                        m_powerbarDirection = 1.f;

                        m_state = State::Stroke;
                        m_doubleTapClock.restart();
                        beginIcon();
                    }
                }
            }
        }
            break;
        case State::Stroke:
        {
            if ((m_inputFlags & InputFlag::Cancel) && ((m_prevFlags & InputFlag::Cancel) == 0))
            {
                m_state = State::Aim;
                resetPower();
                break;
            }

            float speed = dt * MinBarSpeed;
            if (m_terrain == TerrainID::Green
                && m_sharedData.showPuttingPower)
            {
                //speed /= SpeedReduction;
                speed *= 0.95f;
            }
            else
            {
                float increase = std::min(1.f, static_cast<float>(Social::getLevel()) / 10.f);
                speed += ((dt * (1.f - MinBarSpeed)) * increase);
            }

            m_hook = std::min(1.f, std::max(0.f, m_hook + ((speed * m_powerbarDirection))));

            if (m_hook == 1)
            {
                m_powerbarDirection = -1.f;
            }

            if (m_hook == 0
                || ((m_inputFlags & InputFlag::Action) && ((m_prevFlags & InputFlag::Action) == 0)))
            {
                if (m_doubleTapClock.elapsed() > DoubleTapTime)
                {
                    //setActive(false); //can't set this false here because it resets the values before we read them...
                    m_powerbarDirection = 1.f;
                    m_state = State::Flight;

                    auto* msg = cro::App::postMessage<GolfEvent>(MessageID::GolfMessage);
                    msg->type = GolfEvent::HitBall;

                    m_doubleTapClock.restart();

                    endIcon();
                }
            }
        }
            break;
        case State::Flight:
            //do nothing, player's turn is complete
            break;
        }

        //check the filtered inputs
        if (disabledFlags)
        {
            for (auto i = 0u; i < 8u; ++i)
            {
                auto flag = (1 << i);
                if ((disabledFlags & flag)
                    && (m_prevDisabledFlags & flag) == 0)
                {
                    //button was pressed
                    auto* msg = cro::App::postMessage<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::InputActivated;
                    msg->data = flag;
                }
            }
        }
    }

    m_prevDisabledFlags = disabledFlags;
}

void InputParser::updateDroneCam(float dt)
{
    auto rotation = getRotationalInput(cro::GameController::AxisRightX, cro::GameController::AxisRightY);

    float zoom = 0.f;
    if (m_inputFlags & InputFlag::PrevClub)
    {
        zoom -= dt;
    }
    if (m_inputFlags & InputFlag::NextClub)
    {
        zoom += dt;
    }

    cro::Command cmd;
    cmd.targetFlags = CommandID::DroneCam;
    cmd.action = [rotation, zoom](cro::Entity e, float dt)
    {
        auto& zd = e.getComponent<CameraFollower::ZoomData>();
        if (zoom != 0)
        {
            zd.progress = std::clamp(zd.progress + zoom, 0.f, 1.f);
            zd.fov = glm::mix(1.f, zd.target, cro::Util::Easing::easeOutExpo(cro::Util::Easing::easeInQuad(zd.progress)));
            e.getComponent<cro::Camera>().resizeCallback(e.getComponent<cro::Camera>());
        }

        //move more slowly when zoomed in
        float zoomSpeed = 1.f - zd.progress;
        zoomSpeed = 0.15f + (0.85f * zoomSpeed);

        auto& tx = e.getComponent<cro::Transform>();
        auto invRotation = glm::inverse(tx.getRotation());
        auto up = invRotation * cro::Transform::Y_AXIS;

        e.getComponent<cro::Transform>().rotate(up, rotation.y * zoomSpeed * dt);
        e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, rotation.x * zoomSpeed * dt);
    };
    m_gameScene->getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void InputParser::updateSpin(float dt)
{
    auto rotation = getRotationalInput(cro::GameController::AxisLeftX, cro::GameController::AxisLeftY) * 2.f;
    m_spin.x = std::clamp(m_spin.x + (rotation.y * dt), -1.f, 1.f);
    m_spin.y = std::clamp(m_spin.y + (rotation.x * dt), -1.f, 1.f);

    updateDistanceEstimation();
}

void InputParser::rotate(float rotation)
{
    float total = m_rotation + rotation;
    float overspill = 0.f;

    if (total > m_maxRotation)
    {
        overspill = total - m_maxRotation;
        m_rotation = m_maxRotation;
    }
    else if (total < -m_maxRotation)
    {
        overspill = total + m_maxRotation;
        m_rotation = -m_maxRotation;
    }
    else
    {
        m_rotation = total;
    }

    if (!m_isCPU)
    {
        m_holeDirection += overspill;

        //if (overspill != 0)
        {
            auto* msg = cro::App::postMessage<SceneEvent>(MessageID::SceneMessage);
            msg->type = SceneEvent::PlayerRotate;
            msg->rotation = m_holeDirection - (cro::Util::Const::PI / 2.f); //probably not necessary as we could read this directly? Meh.
            msg->rotationPercent = std::abs(m_rotation) / m_maxRotation;
        }
    }
}

void InputParser::checkControllerInput()
{
    //default amount from keyboard input, is overwritten
    //by controller if there is any controller input.
    m_analogueAmount = MinAcceleration + ((1.f - MinAcceleration) * m_inputAcceleration);

    if (m_isCPU ||
        cro::GameController::getControllerCount() == 0
        || m_swingput.isActive())
    {
        return;
    }

    auto controllerID = activeControllerID(m_inputBinding.playerID);

    //left stick
    auto startInput = m_inputFlags;
    float xPos = cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisLeftX);
    //xPos += cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisRightX);

    if (xPos < -LeftThumbDeadZone)
    {
        m_inputFlags |= InputFlag::Left;
    }
    else if (m_prevStick & InputFlag::Left)
    {
        m_inputFlags &= ~InputFlag::Left;
    }

    if (xPos > LeftThumbDeadZone)
    {
        m_inputFlags |= InputFlag::Right;
    }
    else if (m_prevStick & InputFlag::Right)
    {
        m_inputFlags &= ~InputFlag::Right;
    }

    float yPos = cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisLeftY);
    
    float len2 = (xPos * xPos) + (yPos * yPos);
    static const float MinLen2 = static_cast<float>(LeftThumbDeadZone * LeftThumbDeadZone);
    if (len2 > MinLen2)
    {
        m_analogueAmount = std::min(1.f, std::pow(std::sqrt(len2) / (cro::GameController::AxisMax), 5.f));
    }


    //this isn't really analogue, it just moves the camera so do it separately
    yPos = cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisRightY);
    if (yPos > (LeftThumbDeadZone))
    {
        m_inputFlags |= InputFlag::Down;
        m_inputFlags &= ~InputFlag::Up;
    }
    else if (m_prevStick & InputFlag::Down)
    {
        m_inputFlags &= ~InputFlag::Down;
    }

    if (yPos < (-LeftThumbDeadZone))
    {
        m_inputFlags |= InputFlag::Up;
        m_inputFlags &= ~InputFlag::Down;
    }
    else if (m_prevStick & InputFlag::Up)
    {
        m_inputFlags &= ~InputFlag::Up;
    }

    if (startInput ^ m_inputFlags)
    {
        m_prevStick = m_inputFlags;
    }
}

void InputParser::checkMouseInput()
{
    if (m_mouseWheel > 0)
    {
        m_inputFlags |= InputFlag::NextClub;
    }
    else if (m_mouseWheel < 0)
    {
        m_inputFlags |= InputFlag::PrevClub;
    }
    else if (m_prevMouseWheel > 0)
    {
        m_inputFlags &= ~InputFlag::NextClub;
    }
    else if (m_prevMouseWheel < 0)
    {
        m_inputFlags &= ~InputFlag::PrevClub;
    }

    m_prevMouseWheel = m_mouseWheel;
    m_mouseWheel = 0;

    //TODO make this not crap.

    //if (m_mouseMove > 0)
    //{
    //    m_inputFlags |= InputFlag::Right;
    //}
    //else if (m_mouseMove < 0)
    //{
    //    m_inputFlags |= InputFlag::Left;
    //}
    //else if (m_prevMouseMove > 0)
    //{
    //    m_inputFlags &= ~InputFlag::Right;
    //}
    //else if (m_prevMouseMove < 0)
    //{
    //    m_inputFlags &= ~InputFlag::Left;
    //}
    //m_prevMouseMove = m_mouseMove;
    //m_mouseMove = 0;
}

glm::vec2 InputParser::getRotationalInput(std::int32_t xAxis, std::int32_t yAxis) const
{
    glm::vec2 rotation(0.f);
    if (m_inputFlags & (InputFlag::Left | InputFlag::Right))
    {
        if (m_inputFlags & InputFlag::Left)
        {
            rotation.y += 1.f;
        }
        if (m_inputFlags & InputFlag::Right)
        {
            rotation.y -= 1.f;
        }
    }

    if (m_inputFlags & (InputFlag::Up | InputFlag::Down))
    {
        if (m_inputFlags & InputFlag::Down)
        {
            rotation.x -= 1.f;
        }
        if (m_inputFlags & InputFlag::Up)
        {
            rotation.x += 1.f;
        }
    }

    auto controllerID = activeControllerID(m_inputBinding.playerID);
    auto controllerX = cro::GameController::getAxisPosition(controllerID, xAxis);
    auto controllerY = cro::GameController::getAxisPosition(controllerID, yAxis);
    if (std::abs(controllerX) > LeftThumbDeadZone)
    {
        //hmmm we want to read axis inversion from the settings...
        rotation.y = -(static_cast<float>(controllerX) / cro::GameController::AxisMax);
    }
    if (std::abs(controllerY) > LeftThumbDeadZone)
    {
        rotation.x = -(static_cast<float>(controllerY) / cro::GameController::AxisMax);
    }

    if (auto len2 = glm::length2(rotation); len2 != 0)
    {
        rotation = glm::normalize(rotation) * std::min(1.f, std::pow(std::sqrt(len2), 5.f));
    }

    return rotation;
}

void InputParser::beginIcon()
{
    //this is false if we're on the driving range
    if (!m_isCPU &&
        !m_iconActive &&
        m_sharedData.clientConnection.connected)
    {
        Activity a;
        a.type = Activity::PlayerThinkStart;
        a.client = m_sharedData.clientConnection.connectionID;

        m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        m_iconActive = true;
    }
    m_iconTimer.restart();
}

void InputParser::endIcon()
{
    if (!m_isCPU &&
        m_sharedData.clientConnection.connected)
    {
        Activity a;
        a.type = Activity::PlayerThinkEnd;
        a.client = m_sharedData.clientConnection.connectionID;

        m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }

    m_iconActive = false;
}