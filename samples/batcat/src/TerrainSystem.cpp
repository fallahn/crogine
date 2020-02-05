/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "TerrainSystem.hpp"
#include "TerrainChunk.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/core/Clock.hpp>

namespace
{
    const float terrainSpeed = 16.f; //TODO make a variable
}

TerrainSystem::TerrainSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(TerrainSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<TerrainChunk>();
}

//public
void TerrainSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& chunk = entity.getComponent<TerrainChunk>();
        if (chunk.inUse)
        {
            auto& xForm = entity.getComponent<cro::Transform>();
            xForm.move({ -terrainSpeed * dt, 0.f, 0.f });

            //this assumes all chunks have their origin in the centre.
            if (xForm.getPosition().x < 0 && !chunk.followed)
            {
                //enable next chunk
                auto nextEnt = std::find_if(std::begin(entities), std::end(entities), 
                    [](const cro::Entity e)
                {
                    return !e.getComponent<TerrainChunk>().inUse;
                });

                if (nextEnt != entities.end())
                {
                    nextEnt->getComponent<cro::Transform>().setPosition({ chunk.width + xForm.getPosition().x, 0.f, 0.f });
                    nextEnt->getComponent<TerrainChunk>().inUse = true;
                    chunk.followed = true;
                }           
            }

            if (xForm.getPosition().x < -chunk.width)
            {
                //off screen so disable
                chunk.inUse = false;
                chunk.followed = false;
            }
        }
    }
}

//private
void TerrainSystem::onEntityAdded(cro::Entity)
{

}