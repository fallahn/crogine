/*-----------------------------------------------------------------------

Matt Marchant 2021
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
#include <crogine/ecs/Director.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <vector>

/*
\brief Creates a recycled pool of Entity objects
with attached AudioEmitters.

Inherit this class and implement handleMessage() to
trigger specific sound effects on incoming events.

Use getNextEntity() to retrieve the next available
AudioEmtter, set a suitable AudioSource.

\begincode
if(msg.id == PlayerMessage)
{
    const auto& data = msg.getData<PlayerEvent>();
    
    auto emitter = getNextEntity();
    emitter.getComponent<cro::Transform>().setPosition(data.position);

    switch(data.type)
    {
    case PlayerEvent::Died:
        emitter.getComponent<cro::AudioEmitter>().setSource(m_audioResource.get(AudioID::Explosion));
        break;
    case PlayerEvent::Respawned:
        emitter.getComponent<cro::AudioEmitter>().setSource(m_audioResource.get(AudioID::Sparkles));
        break;
    }
    emitter.getComponent<cro::AudioEmitter>().play();
}
\endcode
*/

class SoundEffectsDirector : public cro::Director, public cro::GuiClient
{
public:
    SoundEffectsDirector();

    virtual void handleMessage(const cro::Message&) override = 0;

    void process(float) override;

protected:
    cro::Entity getNextEntity();

private:

    std::size_t m_nextFreeEmitter;
    std::vector<cro::Entity> m_emitters;

    void resizeEmitters();
};