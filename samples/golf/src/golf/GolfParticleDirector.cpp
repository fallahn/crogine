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

#include "GolfParticleDirector.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/graphics/TextureResource.hpp>

GolfParticleDirector::GolfParticleDirector(cro::TextureResource& tr)
{
    m_emitterSettings[ParticleID::Water].loadFromFile("assets/golf/particles/water.cps", tr);
    m_emitterSettings[ParticleID::Grass].loadFromFile("assets/golf/particles/dirt.cps", tr);
    m_emitterSettings[ParticleID::Sand].loadFromFile("assets/golf/particles/sand.cps", tr);

    //hmm how to set smoothing on the texture?
}

//public
void GolfParticleDirector::handleMessage(const cro::Message& msg)
{
    const auto getEnt = [&](std::int32_t id, glm::vec3 position)
    {
        auto entity = getNextEntity();
        entity.getComponent<cro::Transform>().setPosition(position);
        entity.getComponent<cro::ParticleEmitter>().settings = m_emitterSettings[id];
        entity.getComponent<cro::ParticleEmitter>().start();
        return entity;
    };

    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::ClubSwing)
        {
            switch (data.terrain)
            {
            default: break;
            case TerrainID::Rough:
            case TerrainID::Fairway:
                getEnt(ParticleID::Grass, data.position);
                break;
            case TerrainID::Bunker:
                getEnt(ParticleID::Sand, data.position);
                break;
            case TerrainID::Water:
                getEnt(ParticleID::Water, data.position);
                break;
            }
        }
    }
        break;
    case MessageID::CollisionMessage:
    {
        const auto& data = msg.getData<CollisionEvent>();

        if (data.type == CollisionEvent::Begin)
        {
            switch (data.terrain)
            {
            default: break;
            case TerrainID::Rough:
                getEnt(ParticleID::Grass, data.position);
                break;
            case TerrainID::Bunker:
                getEnt(ParticleID::Sand, data.position);
                break;
            case TerrainID::Water:
                getEnt(ParticleID::Water, data.position);
                break;
            }
        }
    }
    break;
    }
}