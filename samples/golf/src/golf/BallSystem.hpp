/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "Terrain.hpp"

#include <crogine/ecs/System.hpp>

#include <crogine/core/Clock.hpp>

namespace cro
{
    class Image;
}

struct Ball final
{
    static constexpr float Radius = 0.0215f;
    enum class State
    {
        Idle, Flight, Putt, Paused, Reset
    }state = State::Idle;

    std::uint8_t terrain = TerrainID::Fairway;

    glm::vec3 velocity = glm::vec3(0.f);
    float delay = 0.f;

    glm::vec3 startPoint = glm::vec3(0.f);
};

class BallSystem final : public cro::System
{
public:
    BallSystem(cro::MessageBus&, const cro::Image&);

    void process(float) override;

    glm::vec3 getWindDirection() const;

    void setHoleData(const struct HoleData&);

private:

    const cro::Image& m_mapData;

    cro::Clock m_windDirClock;
    cro::Clock m_windStrengthClock;

    cro::Time m_windDirTime;
    cro::Time m_windStrengthTime;

    glm::vec3 m_windDirection;
    glm::vec3 m_windDirSrc;
    glm::vec3 m_windDirTarget;

    float m_windStrength;
    float m_windStrengthSrc;
    float m_windStrengthTarget;

    float m_windInterpTime;
    float m_currentWindInterpTime;

    const HoleData* m_holeData;

    void doCollision(cro::Entity);
    void updateWind();
    std::pair<std::uint8_t, glm::vec3> getTerrain(glm::vec3) const;
};