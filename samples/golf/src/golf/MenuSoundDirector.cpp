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

#include "MenuSoundDirector.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"
#include "MenuState.hpp"

#include <crogine/audio/AudioResource.hpp>

#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/util/Random.hpp>

MenuSoundDirector::MenuSoundDirector(cro::AudioResource& ar, const std::size_t& currentMenu)
    : m_currentMenu(currentMenu)
{
    //this must match with AudioID enum
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/ball/holed.wav",
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
void MenuSoundDirector::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case cro::Message::SpriteAnimationMessage:
    {
        if (m_currentMenu == MenuState::MenuID::Lobby)
        {
            const auto& data = msg.getData<cro::Message::SpriteAnimationEvent>();
            auto ent = getNextEntity();

            switch (data.userType)
            {
            default: break;
            case 0:
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[AudioID::Ground]);
                ent.getComponent<cro::AudioEmitter>().setVolume(1.4f);
                break;
            case 1:
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[AudioID::Hole]);
                break;
            }
            ent.getComponent<cro::AudioEmitter>().play();
            ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
        }
    }
        break;
    }
}