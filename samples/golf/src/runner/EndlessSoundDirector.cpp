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

#include "../golf/GameConsts.hpp"

#include "EndlessSoundDirector.hpp"
#include "EndlessMessages.hpp"
#include "TrackSprite.hpp"

#include <crogine/audio/AudioResource.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/util/Random.hpp>

EndlessSoundDirector::EndlessSoundDirector(cro::AudioResource& ar)
{
    //this must match with AudioID enum
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/ball/holed.wav",
        "assets/golf/sound/ball/drop.wav",
        "assets/golf/sound/ball/drop.wav",
        "assets/golf/sound/ball/drop.wav",
    };

    std::fill(m_audioSources.begin(), m_audioSources.end(), nullptr);
    for (auto i = 0; i < AudioID::Count; ++i)
    {
        auto id = ar.load(FilePaths[i]);
        if (id != -1)
        {
            m_audioSources[i] = &ar.get(id);
        }
        else
        {
            //get a default sound so we at least don't have nullptr
            m_audioSources[i] = &ar.get(1010101);
        }
    }
}

//public
void EndlessSoundDirector::handleMessage(const cro::Message& msg)
{
    if (cro::AudioMixer::hasAudioRenderer())
    {
        switch (msg.id)
        {
        default: break;
        case els::MessageID::CollisionMessage:
        {
            const auto& data = msg.getData<els::CollisionEvent>();
            switch (data.id)
            {
            default: break;
            case TrackSprite::Ball:

                break;
            case TrackSprite::Flag:

                break;
            case TrackSprite::Bush01:
            case TrackSprite::TallTree01:
            case TrackSprite::TallTree02:

                break;
            case TrackSprite::CartAway:
            case TrackSprite::CartFront:
            case TrackSprite::CartSide:

                break;
            }
        }

        break;
        }
    }
}

//private
void EndlessSoundDirector::playSound(std::int32_t id, float vol)
{
    auto ent = getNextEntity();
    ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
    ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[id]);
    ent.getComponent<cro::AudioEmitter>().setVolume(vol);
    ent.getComponent<cro::AudioEmitter>().play();
}