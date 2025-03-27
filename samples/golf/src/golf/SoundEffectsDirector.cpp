/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "SoundEffectsDirector.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    const std::size_t MinEmitters = 8;
}

SoundEffectsDirector::SoundEffectsDirector()
    : m_nextFreeEmitter(0)
{
#ifdef CRO_DEBUG_
    /*registerWindow([&]()
        {
            if (ImGui::Begin("Flaps"))
            {
                ImGui::Text("Emitters Size %u", m_emitters.size());
                ImGui::Text("Next Free Emitter %u", m_nextFreeEmitter);
            }
            ImGui::End();        
        });*/
#endif
}

//public
void SoundEffectsDirector::process(float)
{
    //check for finished emitters then free up by swapping
    for (auto i = 0u; i < m_nextFreeEmitter; ++i)
    {
        if (m_emitters[i].getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
        {
            auto entity = m_emitters[i];

            m_nextFreeEmitter--;
            m_emitters[i] = m_emitters[m_nextFreeEmitter];
            m_emitters[m_nextFreeEmitter] = entity;
            i--;
        }
    }
}

//private
void SoundEffectsDirector::resizeEmitters()
{
    m_emitters.resize(m_emitters.size() + MinEmitters);
    for (auto i = m_emitters.size() - MinEmitters; i < m_emitters.size(); ++i)
    {
        m_emitters[i] = getScene().createEntity();
        m_emitters[i].addComponent<cro::AudioEmitter>();
        m_emitters[i].addComponent<cro::Transform>();
    }
}

cro::Entity SoundEffectsDirector::getNextEntity()
{
    if (m_nextFreeEmitter == m_emitters.size())
    {
        resizeEmitters();
    }
    return m_emitters[m_nextFreeEmitter++];
}
