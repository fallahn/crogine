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

struct ControllerRotation final
{
    float rotation = 0.f; //rads
    bool* activeCamera = nullptr;
};

class BilliardsInput final
{
public:
    explicit BilliardsInput(const InputBinding&);

    void handleEvent(const cro::Event&);
    void update(float);

    void setActive(bool active) { m_active = active; }
    void setControlEntities(cro::Entity cam, cro::Entity cue);

    std::pair<glm::vec3, glm::vec3> getImpulse() const;

private:
    const InputBinding& m_inputBinding;

    std::uint16_t m_inputFlags;
    std::uint16_t m_prevFlags;

    std::int32_t m_mouseWheel;
    std::int32_t m_prevMouseWheel;
    float m_mouseMove;
    float m_prevMouseMove;

    bool m_active;

    cro::Entity m_camEntity;
    cro::Entity m_cueEntity;
};
