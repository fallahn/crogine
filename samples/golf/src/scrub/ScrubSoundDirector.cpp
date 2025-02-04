/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "ScrubSoundDirector.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/audio/AudioResource.hpp>
#include <crogine/audio/AudioSource.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

ScrubSoundDirector::ScrubSoundDirector()
{

}

//public
void ScrubSoundDirector::loadSounds(const std::vector<std::string>& paths, cro::AudioResource& ar)
{
    for (const auto& path : paths)
    {
        auto id = ar.load(path);
        if (id != -1)
        {
            m_audioSources.push_back(&ar.get(id));
        }
        else
        {
            //get a default sound so we at least don't have nullptr
            m_audioSources.push_back(&ar.get(1010101));
        }
    }
}

cro::Entity ScrubSoundDirector::playSound(std::int32_t idx, std::uint8_t channel, float vol)
{
    //channel 2 = menu sounds
    //channel 3 = voice over

    CRO_ASSERT(idx > -1 && idx < m_audioSources.size(), "");

    auto emitter = getNextEntity();
    emitter.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[idx]);
    emitter.getComponent<cro::AudioEmitter>().setVolume(vol);
    emitter.getComponent<cro::AudioEmitter>().setPitch(1.f);
    emitter.getComponent<cro::AudioEmitter>().setMixerChannel(channel);
    emitter.getComponent<cro::AudioEmitter>().play();

    return emitter;
}

//private