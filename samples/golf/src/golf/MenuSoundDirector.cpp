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

#include "MenuSoundDirector.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"
#include "MenuState.hpp"

#include <crogine/audio/AudioResource.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/util/Random.hpp>

namespace
{
    const cro::Time MinAudioTime = cro::seconds(0.5f);
}

MenuSoundDirector::MenuSoundDirector(cro::AudioResource& ar, const std::size_t& currentMenu)
    : m_currentMenu(currentMenu)
{
    //this must match with AudioID enum
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/ball/holed.wav",
        "assets/golf/sound/ball/drop.wav",
        "assets/golf/sound/menu/snapshot.wav",
        "assets/golf/sound/menu/lobby.wav",
        "assets/golf/sound/menu/lobby_exit.wav",
        "assets/golf/sound/menu/title.wav",
        "assets/golf/sound/woof.wav",

        "assets/golf/sound/ambience/fw01.wav",
        "assets/golf/sound/ambience/fw02.wav",
        "assets/golf/sound/ambience/fw03.wav",

        "assets/golf/sound/menu/bucket/bounce01.wav",
        "assets/golf/sound/menu/bucket/bounce02.wav",
        "assets/golf/sound/menu/bucket/bounce03.wav",
        "assets/golf/sound/menu/bucket/flip01.wav",
        "assets/golf/sound/menu/bucket/flip02.wav",
        "assets/golf/sound/menu/bucket/flip03.wav",
        "assets/golf/sound/menu/bucket/flip04.wav",
        "assets/golf/sound/menu/bucket/land01.wav",
        "assets/golf/sound/menu/bucket/land02.wav",
        "assets/golf/sound/menu/bucket/land03.wav",
        "assets/golf/sound/menu/bucket/spawn.wav",
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
void MenuSoundDirector::handleMessage(const cro::Message& msg)
{
    if (cro::AudioMixer::hasAudioRenderer())
    {
        switch (msg.id)
        {
        default: break;
        case cl::MessageID::CollisionMessage:
        {
            const auto& data = msg.getData<CollisionEvent>();
            if (data.type == CollisionEvent::Begin
                && data.terrain == 1)
            {
                //coin collision from interpolation
                const auto pitchOffset = static_cast<float>(-10, 10) / 100.f;
                auto ent = playSound(AudioID::Bounce01 + cro::Util::Random::value(0, 2), 0.45f);
                ent.getComponent<cro::AudioEmitter>().setPitch(1 + pitchOffset);
            }
        }
            break;
        case cl::MessageID::EnviroMessage:
        {
            const auto& data = msg.getData<EnviroEvent>();
            auto ent = playSound(AudioID::Fw01 + cro::Util::Random::value(0, 2), 0.2f);
            ent.getComponent<cro::AudioEmitter>().setPitch(data.size);
            ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Environment);
            ent.getComponent<cro::AudioEmitter>().setRolloff(0.f);
        }
        break;
        case cro::Message::SkeletalAnimationMessage:
        {
            const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
            if (data.userType == 10)
            {
                auto e = playSound(AudioID::Woof, 0.5f);
                e.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                e.getComponent<cro::AudioEmitter>().setPitch(cro::Util::Random::value(0.9f, 1.1f));
            }
        }
        break;
        case cro::Message::SystemMessage:
        {
            const auto& data = msg.getData<cro::Message::SystemEvent>();
            if (data.type == cro::Message::SystemEvent::ScreenshotTaken)
            {
                playSound(AudioID::Snapshot, 1.f);
            }
        }
        break;
        case cro::Message::SpriteAnimationMessage:
        {
            const auto& data = msg.getData<cro::Message::SpriteAnimationEvent>();
            if (m_currentMenu == MenuState::MenuID::Lobby)
            {
                //these are raised by the small ball animations
                switch (data.userType)
                {
                default: break;
                case 0:
                    playSound(AudioID::Ground, 2.f);
                    break;
                case 1:
                    playSound(AudioID::Hole);
                    break;
                }
            }
            else if (m_currentMenu == MenuState::MenuID::Avatar)
            {
                switch (data.userType)
                {
                default: break;
                case 2:
                    playSound(AudioID::Ground, 2.f);
                    break;
                }
            }

            //things have changed since this was first implemented
            //the above *probably* does nothing but is left there
            //in case removing it breaks something :)
            //the below case should catch everything from every active state
            //while the menu is open
            else
            {
                //this is specifically when the animation plays on the career menu
                switch (data.userType)
                {
                default: break;
                case 3:
                    playSound(AudioID::Ground, 0.5f);
                    break;
                case 4:
                    playSound(AudioID::Hole, 0.5f);
                    break;
                }
            }
        }
        break;
        case cl::MessageID::SystemMessage:
        {
            const auto& data = msg.getData<SystemEvent>();
            if (data.type == SystemEvent::LobbyEntered)
            {
                playSound(AudioID::LobbyJoin, 0.5f);
            }
            else if (data.type == SystemEvent::LobbyExit)
            {
                playSound(AudioID::LobbyExit, 0.5f);
            }
            else if (data.type == SystemEvent::MenuChanged)
            {
                playSound(AudioID::Title, 0.2f);
            }
        }
            break;
        }
    }
}

cro::Entity MenuSoundDirector::playSound(std::int32_t id, float vol)
{
    auto ent = getNextEntity();
    ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
    ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[id]);
    ent.getComponent<cro::AudioEmitter>().setVolume(vol);
    ent.getComponent<cro::AudioEmitter>().setPitch(1.f);

    //only play if the min time since the last occurance of
    //the sound has been met. Ent is automatically recycled otherwise
    if (m_audioTimers[id].elapsed() > MinAudioTime
        || id > AudioID::Fw03)
    {
        ent.getComponent<cro::AudioEmitter>().play();
        m_audioTimers[id].restart();
    }

    return ent;
}