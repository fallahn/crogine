/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#pragma once

#include "InputBinding.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

struct ControllerRotation final
{
    float rotation = 0.f; //rads
    bool* activeCamera = nullptr;
};

class BilliardsInput final
{
public:
    BilliardsInput(const InputBinding&, cro::MessageBus&);

    void handleEvent(const cro::Event&);
    void update(float);

    void setActive(bool active);
    void setControlEntities(cro::Entity cam, cro::Entity cue);

    std::pair<glm::vec3, glm::vec3> getImpulse() const;

    static constexpr float MaxPower = 1.f;
    float getPower() const { return m_power; }

    const glm::quat& getSpinOffset() const { return m_spinOffset; }

private:
    const InputBinding& m_inputBinding;
    cro::MessageBus& m_messageBus;

    std::uint16_t m_inputFlags;
    std::uint16_t m_prevFlags;

    float m_mouseMove;
    float m_prevMouseMove;

    bool m_active;

    float m_power;
    float m_topSpin;
    float m_sideSpin;
    glm::quat m_spinOffset;

    cro::Entity m_camEntity;
    cro::Entity m_cueEntity;
};
