/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#pragma once

#include <crogine/ecs/Director.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/audio/AudioResource.hpp>

#include <crogine/detail/glm/vec3.hpp>

#include <vector>

namespace cro
{
    class AudioSource;
}

/*
The sound effects director inherits the cro::Director interface and can
be used by adding it to a Scene with cro::Scene::addDirector<T>();

The director pools a series of entities with AudioSource components
attached and can be used to dispatch sound effects based on messages
recieved on the message bus. See SoundEffectsDirector.cpp for more
detailed examples.
*/

class SFXDirector final : public cro::Director
{
public:

    SFXDirector();

    void handleEvent(const cro::Event&) override {}
    void handleMessage(const cro::Message&) override;
    void process(float) override;

private:

    cro::AudioResource m_resources;

    std::vector<cro::Entity> m_entities;
    std::size_t m_nextFreeEntity;

    cro::Entity getNextFreeEntity();
    void resizeEntities(std::size_t);
    cro::AudioSource& playSound(std::int32_t, glm::vec3 = glm::vec3(0.f));
};
