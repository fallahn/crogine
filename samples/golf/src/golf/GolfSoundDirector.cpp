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
#include "GameConsts.hpp"
#include "ScoreStrings.hpp"
#include "Clubs.hpp"

#include <crogine/audio/AudioResource.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

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

        "assets/golf/sound/holes/albatross.wav",
        "assets/golf/sound/holes/birdie.wav",
        "assets/golf/sound/holes/bogie.wav",
        "assets/golf/sound/holes/bogie_double.wav",
        "assets/golf/sound/holes/bogie_triple.wav",
        "assets/golf/sound/holes/eagle.wav",
        "assets/golf/sound/holes/hole.wav",
        "assets/golf/sound/holes/par.wav",

        "assets/golf/sound/ball/applause.wav",

        "assets/golf/sound/terrain/bunker01.wav",
        "assets/golf/sound/terrain/bunker02.wav",
        "assets/golf/sound/terrain/bunker03.wav",
        "assets/golf/sound/terrain/bunker04.wav",
        "assets/golf/sound/terrain/bunker05.wav",
        "assets/golf/sound/terrain/fairway01.wav",
        "assets/golf/sound/terrain/fairway02.wav",
        "assets/golf/sound/terrain/green01.wav",
        "assets/golf/sound/terrain/green02.wav",
        "assets/golf/sound/terrain/rough01.wav",
        "assets/golf/sound/terrain/rough02.wav",
        "assets/golf/sound/terrain/scrub01.wav",
        "assets/golf/sound/terrain/scrub02.wav",
        "assets/golf/sound/terrain/scrub03.wav",
        "assets/golf/sound/terrain/scrub04.wav",
        "assets/golf/sound/terrain/water01.wav",
        "assets/golf/sound/terrain/water02.wav",
        "assets/golf/sound/terrain/water03.wav",

        "assets/golf/sound/kudos/swing01.wav",
        "assets/golf/sound/kudos/swing02.wav",
        "assets/golf/sound/kudos/swing03.wav",

        "assets/golf/sound/kudos/hook.wav",
        "assets/golf/sound/kudos/slice.wav",
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
    static constexpr float MinBallDistance = 100.f; //dist sqr
    static constexpr float MinHoleDistance = 9.f; //dist to hole sqr

    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        switch (data.type)
        {
        default: break;
        case GolfEvent::ClubChanged:
            //playSound(cro::Util::Random::value(AudioID::Putt01, AudioID::Putt03), data.position);
            break;
        case GolfEvent::HookedBall:
            //if (glm::length(data.position) > MinBallDistance)
            {
                playSound(AudioID::Hook, glm::vec3(0.f));
            }
            break;
        case GolfEvent::SlicedBall:
            //if (glm::length(data.position) > MinBallDistance)
            {
                playSound(AudioID::Slice, glm::vec3(0.f));
            }
            break;
        case GolfEvent::NiceShot:
            playSound(cro::Util::Random::value(AudioID::NiceSwing01, AudioID::NiceSwing03), data.position);
            break;
        case GolfEvent::ClubSwing:
        {
            if (data.terrain == TerrainID::Green)
            {
                playSound(cro::Util::Random::value(AudioID::Putt01, AudioID::Putt03), data.position);
            }
            else if (data.terrain == TerrainID::Bunker)
            {
                playSound(AudioID::Wedge, data.position);
            }
            else
            {
                playSound(cro::Util::Random::value(AudioID::Swing01, AudioID::Swing03), data.position);
            }
        }
            break;
        case GolfEvent::Scored:
            switch (data.score)
            {
            default: 
                //TODO this is probably way over par so play some sad sound
                playSound(AudioID::ScoreHole, glm::vec3(0.f));
                break; 
            case ScoreID::Albatross:
                playSound(AudioID::ScoreAlbatross, glm::vec3(0.f));
                break;
            case ScoreID::Birdie:
                playSound(AudioID::ScoreBirdie, glm::vec3(0.f));
                break;
            case ScoreID::Bogie:
                playSound(AudioID::ScoreBogie, glm::vec3(0.f));
                break;
            case ScoreID::DoubleBogie:
                playSound(AudioID::ScoreDoubleBogie, glm::vec3(0.f));
                break;
            case ScoreID::Eagle:
                playSound(AudioID::ScoreEagle, glm::vec3(0.f));
                break;
            case ScoreID::Par:
                playSound(AudioID::ScorePar, glm::vec3(0.f));
                break;
            case ScoreID::TripleBogie:
                playSound(AudioID::ScoreTripleBogie, glm::vec3(0.f));
                break;
            }

            if (data.score <= ScoreID::Par)
            {
                playSound(AudioID::Applause, glm::vec3(0.f));
            }
            break;
        case GolfEvent::BallLanded:
            if (data.travelDistance > MinBallDistance)
            {
                switch (data.terrain)
                {
                default: break;
                case TerrainID::Bunker:
                    playSound(cro::Util::Random::value(AudioID::TerrainBunker01, AudioID::TerrainBunker05), glm::vec3(0.f));
                    break;
                case TerrainID::Fairway:
                    playSound(cro::Util::Random::value(AudioID::TerrainFairway01, AudioID::TerrainFairway02), glm::vec3(0.f));
                    break;
                case TerrainID::Scrub:
                    playSound(cro::Util::Random::value(AudioID::TerrainScrub02, AudioID::TerrainScrub04), glm::vec3(0.f));
                    break;
                case TerrainID::Green:
                    if (data.club != ClubID::Putter) //previous shot wasn't from green
                    {
                        playSound(cro::Util::Random::value(AudioID::TerrainGreen01, AudioID::TerrainGreen02), glm::vec3(0.f));

                        if (data.pinDistance < MinHoleDistance)
                        {
                            playSoundDelayed(AudioID::Applause, glm::vec3(0.f), 0.8f);
                        }
                    }
                    break;
                case TerrainID::Water:
                    playSound(cro::Util::Random::value(AudioID::TerrainWater01, AudioID::TerrainWater03), glm::vec3(0.f));
                    break;
                case TerrainID::Rough:
                    playSound(cro::Util::Random::value(AudioID::TerrainRough01, AudioID::TerrainRough02), glm::vec3(0.f));
                    break;
                }
            }
            else
            {
                if (data.club != ClubID::Putter
                    && cro::Util::Random::value(0, 4) == 0)
                {
                    //you're poop.
                    playSound(AudioID::TerrainScrub01, glm::vec3(0.f));
                }
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
            switch (data.terrain)
            {
            default:
                playSound(AudioID::Ground, data.position);
                break;
            case TerrainID::Water:
                playSound(AudioID::Water, data.position);
                break;
            case TerrainID::Hole:
                playSound(AudioID::Hole, data.position);
                break;
            case TerrainID::Scrub:
                playSound(AudioID::Scrub, data.position);
                break;
            }
        }
    }
        break;
    }
}

//private
void GolfSoundDirector::playSound(std::int32_t id, glm::vec3 position, float volume)
{
    auto ent = getNextEntity();
    ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[id]);
    ent.getComponent<cro::AudioEmitter>().play();
    ent.getComponent<cro::AudioEmitter>().setVolume(volume);
    ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
    ent.getComponent<cro::Transform>().setPosition(position);
}

void GolfSoundDirector::playSoundDelayed(std::int32_t id, glm::vec3 position, float delay, float volume)
{
    auto entity = getScene().createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(delay);
    entity.getComponent<cro::Callback>().function =
        [&, id, position, volume](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0)
        {
            playSound(id, position, volume);

            e.getComponent<cro::Callback>().active = false;
            getScene().destroyEntity(e);
        }
    };
}