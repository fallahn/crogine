/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

/*
NOTE since the introduction of Billiards the collision mesh
classes have become somewhat ambiguous in naming... this class
deals specifically with golf terrain collision on the client.
*/

#include "HoleData.hpp"

#include <crogine/ecs/System.hpp>

#include <crogine/graphics/ImageArray.hpp>

#include <crogine/detail/glm/vec3.hpp>

class CollisionMesh;

struct ClientCollider final
{
    std::int32_t previousDirection = 0;
    float previousHeight = 0.f; //relative to terrain
    float previousWorldHeight = 0.f;
    std::uint8_t terrain = 0;
    std::uint8_t state = 0;
    std::uint8_t lie = 0; //0 buried, 1 sitting up
    bool active = false;
    bool nearHole = false;
    bool hidden = false; //hidden in team mode
};

class ClientCollisionSystem final : public cro::System 
{
public:
    ClientCollisionSystem(cro::MessageBus&, const std::vector<HoleData>&, const CollisionMesh&);

    void process(float) override;

    void setActiveClub(std::int32_t club) { m_club = club; } //hacky.

    void setMap(std::uint32_t);

    void setPinPosition(glm::vec3 p) { m_pinPosition = p; }

private:
    const std::vector<HoleData>& m_holeData;
    std::uint32_t m_holeIndex;
    const CollisionMesh& m_collisionMesh;
    std::int32_t m_club;

    glm::vec3 m_pinPosition;

    cro::ImageArray<std::uint8_t> m_mapImage;

    bool m_waterCollision; //tracking dolphin achievement
};
