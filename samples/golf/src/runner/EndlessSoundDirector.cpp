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
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/util/Random.hpp>

EndlessSoundDirector::EndlessSoundDirector(cro::AudioResource& ar)
{
    //this must match with AudioID enum
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/ball/pole.wav",
        "assets/golf/sound/kudos/flag_pole.wav",
        "assets/golf/sound/tutorial_appear.wav",
        "assets/golf/sound/ball/scrub.wav",
        "assets/golf/sound/bad.wav",

        "assets/golf/sound/menu/star.wav",
        "assets/golf/sound/menu/no_streak.wav",
        "assets/golf/sound/menu/toot.wav",
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
        case els::MessageID::GameMessage:
        {
            const auto& data = msg.getData<els::GameEvent>();
            switch (data.type)
            {
            default: break;
            case els::GameEvent::CrossedLine:
                playSound(AudioID::LapLine, 0.5f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
                break;
            case els::GameEvent::LostStreak:
                playSound(AudioID::StreakLost, 0.2f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
                break;
            case els::GameEvent::Toot:
                playSound(AudioID::Toot, 0.5f).getComponent<cro::AudioEmitter>();
                break;
            }
        }
        break;
        case els::MessageID::CollisionMessage:
        {
            const auto& data = msg.getData<els::CollisionEvent>();
            switch (data.id)
            {
            default: break;
            case TrackSprite::Ball:
                playSound(AudioID::Ball, 0.2f).getComponent<cro::AudioEmitter>().setPitch(cro::Util::Random::value(0.85f, 1.1f));
                break;
            case TrackSprite::Flag:
                playSound(AudioID::Flag);
                playSoundDelayed(AudioID::FlagVoice, 0.4f, MixerChannel::Voice);
                break;
            case TrackSprite::Bush01:
            case TrackSprite::Bush02:
            case TrackSprite::Bush03:
            case TrackSprite::TallTree01:
            case TrackSprite::TallTree02:
                playSound(AudioID::Tree);
                break;
            case TrackSprite::CartAway:
            case TrackSprite::CartFront:
            case TrackSprite::CartSide:
            case TrackSprite::LampPost:
            case TrackSprite::Platform:
            case TrackSprite::PhoneBox:
            case TrackSprite::Tree01:
            case TrackSprite::Tree02:
            case TrackSprite::Tree03:
            case TrackSprite::Log:
            case TrackSprite::Rock:
                playSound(AudioID::Cart);
                break;
            }
        }

        break;
        }
    }
}

//private
cro::Entity EndlessSoundDirector::playSound(std::int32_t id, float vol)
{
    auto ent = getNextEntity();
    ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
    ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[id]);
    ent.getComponent<cro::AudioEmitter>().setVolume(vol);
    ent.getComponent<cro::AudioEmitter>().setPitch(1.f);
    ent.getComponent<cro::AudioEmitter>().play();

    return ent;
}

void EndlessSoundDirector::playSoundDelayed(std::int32_t id, float delay, float volume, std::uint8_t channel)
{
    auto entity = getScene().createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(delay);
    entity.getComponent<cro::Callback>().function =
        [&, id, volume, channel](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime -= dt;

            if (currTime < 0)
            {
                playSound(id, volume).getComponent<cro::AudioEmitter>().setMixerChannel(channel);
                e.getComponent<cro::Callback>().active = false;
                getScene().destroyEntity(e);
            }
        };
}