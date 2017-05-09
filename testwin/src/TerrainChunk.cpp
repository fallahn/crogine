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

#include "TerrainChunk.hpp"
#include "Messages.hpp"

#include <crogine/util/Random.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

ChunkSystem::ChunkSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(ChunkSystem)),
    m_speed         (0.f),
    m_currentSpeed  (0.f),
    m_topX          (cro::Util::Random::value(-2343.f, 5637.f)),
    m_bottomX       (cro::Util::Random::value(-3965.f, 2842.f))
{
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
    requireComponent<TerrainChunk>();

    //TODO build wave tables
}

//public
void ChunkSystem::process(cro::Time dt)
{
    auto& entities = getEntities();

    //move each entity according to current speed
    float dtSec = dt.asSeconds();
    m_currentSpeed += ((m_speed - m_currentSpeed) * dtSec);

    for (auto& e : entities)
    {
        auto& tx = e.getComponent<cro::Transform>();
        tx.move({ -m_currentSpeed * dtSec, 0.f, 0.f });


        //if out of view move by one width and rebuild from current coords
        //assuming origin is in centre this become -chunkWidth

        //first chunk top//bottom should be a gradient in to average height
        //don't forget vertices need colour.
    }
}

void ChunkSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BackgroundSystem)
    {
        const auto& data = msg.getData<BackgroundEvent>();
        if (data.type == BackgroundEvent::SpeedChange)
        {
            m_speed = data.value;
        }
    }
}

//private
void ChunkSystem::rebuildChunk(cro::Entity entity)
{
    //generate points and store in terrain component.
    //these are also used for collision detection


    //build mesh. first half of points are bottom chunk, then rest are for top
}