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

#include "GolfSoundDirector.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"

#include <crogine/audio/AudioResource.hpp>

#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/util/Random.hpp>

GolfSoundDirector::GolfSoundDirector(cro::AudioResource& ar)
{
    //this must match with AudioID enum
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/ball/putt01.wav",
        "assets/golf/sound/ball/putt02.wav",
        "assets/golf/sound/ball/putt03.wav",
        
        "assets/golf/sound/ball/swing01.wav",
        "assets/golf/sound/ball/swing02.wav",
        "assets/golf/sound/ball/swing03.wav",

        "assets/golf/sound/ball/wedge01.wav",

        "assets/golf/sound/ball/holed.wav",
        "assets/golf/sound/ball/splash.wav",
        "assets/golf/sound/ball/drop.wav",
        "assets/golf/sound/ball/scrub.wav",
    };

    std::fill(m_audioSources.begin(), m_audioSources.end(), nullptr);
    for (auto i = 0; i < AudioID::Count; ++i)
    {
        auto id = ar.load(FilePaths[i]); //TODO do we really want to assume none of these are streamed??
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
void GolfSoundDirector::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        switch (data.type)
        {
        default: break;
        case GolfEvent::ClubSwing:
        {
            auto ent = getNextEntity();

            if (data.terrain == TerrainID::Green)
            {
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[cro::Util::Random::value(AudioID::Putt01, AudioID::Putt03)]);
            }
            else if (data.terrain == TerrainID::Bunker)
            {
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[AudioID::Wedge]);
            }
            else
            {
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[cro::Util::Random::value(AudioID::Swing01, AudioID::Swing03)]);
            }
            ent.getComponent<cro::AudioEmitter>().play();
            ent.getComponent<cro::Transform>().setPosition(data.position);
        }
            break;
        }
    }
        break;
    case MessageID::CollisionMessage:
    {
        const auto& data = msg.getData<CollisionEvent>();
        if (data.type == CollisionEvent::Begin)
        {
            auto ent = getNextEntity();
            switch (data.terrain)
            {
            default:
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[AudioID::Ground]);
                break;
            case TerrainID::Water:
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[AudioID::Water]);
                break;
            case TerrainID::Hole:
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[AudioID::Hole]);
                break;
            case TerrainID::Scrub:
                ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[AudioID::Scrub]);
                break;
            }
            ent.getComponent<cro::AudioEmitter>().play();
            ent.getComponent<cro::Transform>().setPosition(data.position);
        }
    }
        break;
    }
}