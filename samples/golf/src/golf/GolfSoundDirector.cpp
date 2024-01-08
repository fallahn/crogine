/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "GolfSoundDirector.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"
#include "ScoreStrings.hpp"
#include "Clubs.hpp"
#include "CommandIDs.hpp"
#include "VatAnimationSystem.hpp"
#include "XPAwardStrings.hpp"

#include <crogine/audio/AudioResource.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/util/Random.hpp>

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

using namespace cl;

namespace
{
    constexpr float VoiceDelay = 0.5f;

    const cro::Time MinCrowdTime = cro::seconds(43.f);
    const cro::Time FlagSoundTime = cro::seconds(3.f);
    const cro::Time ChatSoundTime = cro::seconds(0.05f);
    const cro::Time PowerSoundTime = cro::seconds(0.5f);
    const cro::Time ApplauseSoundTime = cro::seconds(3.5f);
}

GolfSoundDirector::GolfSoundDirector(cro::AudioResource& ar, const SharedStateData& sd)
    : m_sharedData      (sd),
    m_currentClient     (0),
    m_currentPlayer     (0),
    m_honourID          (0),
    m_newHole           (false),
    m_crowdPositions    (nullptr),
    m_crowdTime         (MinCrowdTime)
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
        "assets/golf/sound/ball/near_holed.wav",
        "assets/golf/sound/ball/near_miss.wav",
        "assets/golf/sound/ball/splash.wav",
        "assets/golf/sound/ball/drop.wav",
        "assets/golf/sound/ball/scrub.wav",
        "assets/golf/sound/ball/stone.wav",
        "assets/golf/sound/ball/pole.wav",
        "assets/golf/sound/ball/power.wav",

        "assets/golf/sound/holes/albatross.wav",
        "assets/golf/sound/holes/birdie.wav",
        "assets/golf/sound/holes/bogie.wav",
        "assets/golf/sound/holes/bogie_double.wav",
        "assets/golf/sound/holes/bogie_triple.wav",
        "assets/golf/sound/holes/eagle.wav",
        "assets/golf/sound/holes/hole.wav",
        "assets/golf/sound/holes/par.wav",
        "assets/golf/sound/holes/hio.wav",
        "assets/golf/sound/holes/in_one.wav",

        "assets/golf/sound/holes/draw01.wav",
        "assets/golf/sound/holes/draw02.wav",
        "assets/golf/sound/billiards/announcer/win.wav",
        "assets/golf/sound/billiards/announcer/lose.wav",

        "assets/golf/sound/ball/applause.wav",
        "assets/golf/sound/ball/applause_plus.wav",

        "assets/golf/sound/terrain/bunker01.wav",
        "assets/golf/sound/terrain/bunker02.wav",
        "assets/golf/sound/terrain/bunker03.wav",
        "assets/golf/sound/terrain/bunker04.wav",
        "assets/golf/sound/terrain/bunker05.wav",
        "assets/golf/sound/terrain/fairway01.wav",
        "assets/golf/sound/terrain/fairway02.wav",
        "assets/golf/sound/terrain/green01.wav",
        "assets/golf/sound/terrain/green02.wav",
        "assets/golf/sound/terrain/green03.wav",
        "assets/golf/sound/terrain/rough01.wav",
        "assets/golf/sound/terrain/rough02.wav",
        "assets/golf/sound/terrain/scrub01.wav",
        "assets/golf/sound/terrain/scrub02.wav",
        "assets/golf/sound/terrain/scrub03.wav",
        "assets/golf/sound/terrain/scrub04.wav",
        "assets/golf/sound/terrain/water01.wav",
        "assets/golf/sound/terrain/water02.wav",
        "assets/golf/sound/terrain/water03.wav",
        "assets/golf/sound/terrain/target.wav",

        "assets/golf/sound/holes/honour.wav",
        "assets/golf/sound/kudos/swing01.wav",
        "assets/golf/sound/kudos/swing02.wav",
        "assets/golf/sound/kudos/swing03.wav",
        "assets/golf/sound/kudos/power_drive.wav",
        "assets/golf/sound/kudos/power_drive02.wav",
        "assets/golf/sound/kudos/power_drive03.wav",
        "assets/golf/sound/kudos/power_drive04.wav",
        "assets/golf/sound/kudos/flag_pole.wav",

        "assets/golf/sound/kudos/hook.wav",
        "assets/golf/sound/kudos/slice.wav",

        "assets/golf/sound/kudos/drive_excellent.wav",
        "assets/golf/sound/kudos/drive_good.wav",
        "assets/golf/sound/kudos/drive_poor.wav",

        "assets/golf/sound/kudos/putt02.wav",
        "assets/golf/sound/kudos/putt01.wav",
        "assets/golf/sound/holes/gimme.wav",

        "assets/golf/sound/ambience/firework.wav",
        "assets/golf/sound/ambience/burst.wav",
        "assets/golf/sound/holes/airmail.wav",
        "assets/golf/sound/ambience/birds01.wav",
        "assets/golf/sound/ambience/billboard_swing.wav",
        "assets/golf/sound/ambience/billboard_rewind.wav",

        "assets/golf/sound/ambience/foot01.wav",
        "assets/golf/sound/ambience/foot02.wav",
        "assets/golf/sound/ambience/foot03.wav",
        "assets/golf/sound/ambience/foot04.wav",

        "assets/golf/sound/bad.wav",
        "assets/golf/sound/tutorial_appear.wav",
        "assets/golf/sound/menu/poke.wav",
        "assets/golf/sound/menu/skins.wav",

        "assets/golf/sound/ambience/crowd_clear_throat.wav",
        "assets/golf/sound/ambience/crowd_cough.wav",
        "assets/golf/sound/ambience/crowd_sigh.wav",
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

    for (auto& a : m_playerIndices)
    {
        std::fill(a.begin(), a.end(), -1);
    }
}

//public
void GolfSoundDirector::handleMessage(const cro::Message& msg)
{
    static constexpr float MinBallDistance = 100.f; //dist sqr
    static constexpr float MinHoleDistance = 9.f; //dist to hole sqr

    if (cro::AudioMixer::hasAudioRenderer())
    {
        switch (msg.id)
        {
        default: break;
        case Social::MessageID::SocialMessage:
        {
            const auto& data = msg.getData<Social::SocialEvent>();
            if (data.type == Social::SocialEvent::LevelUp
                || (data.type == Social::SocialEvent::XPAwarded && data.reason == XPStringID::ChallengeComplete))
            {
                if (m_soundTimers[AudioID::ApplausePlus].elapsed() > ApplauseSoundTime)
                {
                    playSound(AudioID::ApplausePlus, glm::vec3(0.f)).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
            }
            else if (data.type == Social::SocialEvent::PlayerAchievement)
            {
                auto id = data.level == 0 ? AudioID::ApplausePlus : AudioID::NearMiss;
                if (m_soundTimers[AudioID::ApplausePlus].elapsed() > ApplauseSoundTime)
                {
                    playSound(id, glm::vec3(0.f)).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
            }
        }
            break;
        case cro::Message::SpriteAnimationMessage:
        {
            const auto& data = msg.getData<cro::Message::SpriteAnimationEvent>();
            if (data.userType == SpriteAnimID::Medal) //scoreboard star animation
            {
                playSound(cro::Util::Random::value(AudioID::Swing01, AudioID::Swing03), glm::vec3(0.f)).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
            }
        }
        break;
        case cro::Message::SkeletalAnimationMessage:
        {
            const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
            if (data.userType == SpriteAnimID::BillboardRewind)
            {
                auto sound = playSound(AudioID::BillboardRewind, data.position, 1.2f);
                sound.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                sound.getComponent<cro::AudioEmitter>().setRolloff(0.65f);
            }
            else if (data.userType == SpriteAnimID::BillboardSwing)
            {
                auto sound = playSound(AudioID::BillboardSwing, data.position, 1.2f);
                sound.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                sound.getComponent<cro::AudioEmitter>().setRolloff(0.65f);
            }
            else if (data.userType == SpriteAnimID::Footstep)
            {
                auto sound = playSound(cro::Util::Random::value(AudioID::Foot01, AudioID::Foot04), data.position, 0.3f);
                sound.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                sound.getComponent<cro::AudioEmitter>().setRolloff(0.74f);
                sound.getComponent<cro::AudioEmitter>().setPitch(1.f + cro::Util::Random::value(-0.2f, 0.2f));
            }
        }
        break;
        case MessageID::GolfMessage:
        {
            const auto& data = msg.getData<GolfEvent>();
            switch (data.type)
            {
            default: break;
            case GolfEvent::TargetHit:
                playSound(AudioID::Target, data.position, 0.2f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                break;
            case GolfEvent::RoundEnd:
                if (data.score == 0)
                {
                    playSoundDelayed(AudioID::Win, data.position, 1.6f, 1.f, MixerChannel::Voice);
                }
                else
                {
                    playSoundDelayed(AudioID::Lose, data.position, 1.6f, 1.f, MixerChannel::Voice);
                }
                break;
            case GolfEvent::Gimme:
                //if (cro::Util::Random::value(0, 2) != 0)
            {
                playSoundDelayed(AudioID::Gimme, data.position, 1.6f, 1.f, MixerChannel::Voice);
            }
            break;
            case GolfEvent::BirdHit:
                playSound(AudioID::Birds, data.position, 1.6f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                break;
            case GolfEvent::DroneHit:
                playSound(AudioID::Burst, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                playSoundDelayed(AudioID::Airmail, glm::vec3(0.f), 1.2f, 1.f, MixerChannel::Voice);
                break;
            case GolfEvent::HoleWon:
                if (auto idx = m_playerIndices[data.client][data.player]; idx > -1)
                {
                    static const std::string emitterName = "celebrate";
                    if (m_playerVoices[idx].hasEmitter(emitterName))
                    {
                        playAvatarSound(idx, emitterName, glm::vec3(0.f));
                    }
                }

                if (m_sharedData.scoreType == ScoreType::Skins)
                {
                    playSoundDelayed(AudioID::SkinsWin, glm::vec3(0.f), 1.f, 1.f, MixerChannel::Menu);
                }

                break;
            case GolfEvent::HoleDrawn:
                playSoundDelayed(cro::Util::Random::value(AudioID::Draw01, AudioID::Draw02), glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                break;
            case GolfEvent::HoleInOne:
                playSound(AudioID::ScoreHIO, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                break;
            case GolfEvent::DriveComplete:
                if (cro::Util::Random::value(0, 2) != 0)
                {
                    switch (data.score)
                    {
                    default: break;
                    case 1:
                        playSoundDelayed(AudioID::DriveBad, glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                        break;
                    case 2:
                        playSoundDelayed(AudioID::DriveGood, glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                        break;
                    case 3:
                        playSoundDelayed(AudioID::DriveExcellent, glm::vec3(0.f), 0.2f, 1.f, MixerChannel::Voice);
                        break;
                    }
                }
                break;
            case GolfEvent::ClubChanged:
                if (data.score != 0)
                {
                    playSound(AudioID::Putt01, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
                }
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
                playSoundDelayed(cro::Util::Random::value(AudioID::NiceSwing01, AudioID::NiceSwing03), data.position, 0.8f, 1.f, MixerChannel::Voice);
                break;
            case GolfEvent::PowerShot:
                if (m_soundTimers[AudioID::PowerBall].elapsed() > PowerSoundTime)
                {
                    playSound(AudioID::PowerBall, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);

                    //this might also play if a player quits but meh
                    if (cro::Util::Random::value(0, 2) == 0)
                    {
                        playSoundDelayed(AudioID::PowerShot01 + cro::Util::Random::value(0, 3), data.position, 1.5f, 1.1f, MixerChannel::Voice);
                    }
                }
                break;
            case GolfEvent::ClubSwing:
            {
                if (data.terrain == TerrainID::Green)
                {
                    playSound(cro::Util::Random::value(AudioID::Putt01, AudioID::Putt03), data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
                else if (data.terrain == TerrainID::Bunker)
                {
                    playSound(AudioID::Wedge, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
                else
                {
                    playSound(cro::Util::Random::value(AudioID::Swing01, AudioID::Swing03), data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
            }
            break;
            case GolfEvent::Scored:
                switch (data.score)
                {
                default:
                    //playSoundDelayed(AudioID::NearMiss, data.position, 0.5f, 0.4f, MixerChannel::Effects);
                    playSoundDelayed(AudioID::ScoreHole, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::HIO:
                    playSoundDelayed(AudioID::ScoreInOne, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Albatross:
                    playSoundDelayed(AudioID::ScoreAlbatross, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Birdie:
                    playSoundDelayed(AudioID::ScoreBirdie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Bogie:
                    playSoundDelayed(AudioID::ScoreBogie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::DoubleBogie:
                    playSoundDelayed(AudioID::ScoreDoubleBogie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Eagle:
                    playSoundDelayed(AudioID::ScoreEagle, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::Par:
                    playSoundDelayed(AudioID::ScorePar, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                case ScoreID::TripleBogie:
                    playSoundDelayed(AudioID::ScoreTripleBogie, glm::vec3(0.f), VoiceDelay, 1.f, MixerChannel::Voice);
                    break;
                }

                if (data.score <= ScoreID::Par
                    && m_soundTimers[AudioID::Applause].elapsed() > ApplauseSoundTime)
                {
                    playSoundDelayed(AudioID::Applause, glm::vec3(0.f), 0.8f, MixerChannel::Effects);
                    applaud();
                }

                if (data.travelDistance > (LongPuttDistance * LongPuttDistance))
                {
                    if (m_soundTimers[AudioID::ApplausePlus].elapsed() > ApplauseSoundTime)
                    {
                        playSoundDelayed(AudioID::ApplausePlus, glm::vec3(0.f), 1.2f, MixerChannel::Effects);
                        applaud();
                    }

                    //sunk an extra long putt
                    if (data.club == ClubID::Putter)
                    {
                        playSoundDelayed(cro::Util::Random::value(AudioID::NicePutt01, AudioID::NicePutt02), glm::vec3(0.f), 2.2f, 1.f, MixerChannel::Voice);
                    }
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

                        if (auto idx = m_playerIndices[m_currentClient][m_currentPlayer]; idx > -1)
                        {
                            playAvatarSoundDelayed(idx, "scrub", glm::vec3(0.f), 2.f);
                        }

                        break;
                    case TerrainID::Fairway:
                        playSound(cro::Util::Random::value(AudioID::TerrainFairway01, AudioID::TerrainFairway02), glm::vec3(0.f));
                        break;
                    case TerrainID::Scrub:
                        playSound(cro::Util::Random::value(AudioID::TerrainScrub02, AudioID::TerrainScrub04), glm::vec3(0.f));

                        if (auto idx = m_playerIndices[m_currentClient][m_currentPlayer]; idx > -1)
                        {
                            playAvatarSoundDelayed(idx, "scrub", glm::vec3(0.f), 2.2f);
                        }
                        break;
                    case TerrainID::Green:
                        if (data.club != ClubID::Putter) //previous shot wasn't from green
                        {
                            playSound(cro::Util::Random::value(AudioID::TerrainGreen01, AudioID::TerrainGreen03), glm::vec3(0.f));

                            if (data.pinDistance < MinHoleDistance //near the hole
                                || data.travelDistance > 10000.f) //landed a long shot
                            {
                                if (m_soundTimers[AudioID::Applause].elapsed() > ApplauseSoundTime)
                                {
                                    playSoundDelayed(AudioID::Applause, glm::vec3(0.f), 0.8f);
                                    applaud();
                                }
                                Social::awardXP(XPValues[XPID::Par] / 2, XPStringID::NiceOn);
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
                    if (data.travelDistance < 9
                        && data.club != ClubID::Putter
                        && cro::Util::Random::value(0, 4) == 0)
                    {
                        //you're poop.
                        playSound(AudioID::TerrainScrub01, glm::vec3(0.f));
                    }

                    if (data.club == ClubID::Putter
                        && (data.terrain == TerrainID::Water || data.terrain == TerrainID::Scrub))
                    {
                        //assume we putt off the green on a putting course
                        playSound(AudioID::TerrainWater03, glm::vec3(0.f));
                    }

                    if (data.terrain == TerrainID::Hole
                        && data.club != ClubID::Putter)
                    {
                        playSoundDelayed(AudioID::DriveGood, data.position, 2.4f, 1.f, MixerChannel::Voice);
                        if (m_soundTimers[AudioID::ApplausePlus].elapsed() > ApplauseSoundTime)
                        {
                            playSoundDelayed(AudioID::ApplausePlus, data.position, 0.8f, 0.6f, MixerChannel::Effects);
                        }
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
                case CollisionEvent::Billboard:
                    playSound(AudioID::BadSport, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case CollisionEvent::Firework:
                {
                    auto e = playSound(AudioID::Firework, data.position);
                    e.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    e.getComponent<cro::AudioEmitter>().setPitch(cro::Util::Random::value(0.95f, 1.2f));
                }
                    break;
                case CollisionEvent::FlagPole:
                    if (m_soundTimers[AudioID::Pole].elapsed() > FlagSoundTime)
                    {
                        playSound(AudioID::Pole, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                        m_soundTimers[AudioID::Pole].restart();

                        if (m_sharedData.gimmeRadius == 0 &&
                            cro::Util::Random::value(0, 1) == 0)
                        {
                            playSoundDelayed(AudioID::BigStick, data.position, 2.2f, 1.f, MixerChannel::Voice);
                        }
                    }
                    break;
                default:
                    playSound(AudioID::Ground, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Water:
                    playSound(AudioID::Water, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Hole:
                    playSound(AudioID::Hole, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Scrub:
                    playSound(AudioID::Scrub, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                case TerrainID::Stone:
                    playSound(AudioID::Stone, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    break;
                }
            }
            else if (data.type == CollisionEvent::End)
            {
                if (data.terrain == TerrainID::Green
                    && data.clubID == ClubID::Putter)
                {
                    playSound(AudioID::Ground, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
                else if (data.terrain == TerrainID::Hole)
                {
                    playSound(AudioID::Hole, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                }
            }
            else if (data.type == CollisionEvent::NearMiss)
            {
                playSoundDelayed(AudioID::NearMiss, data.position, 0.5f, 1.f, MixerChannel::Effects);
                //playSound(AudioID::NearHole, data.position).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
            }
        }
        break;
        case MessageID::SceneMessage:
        {
            const auto& data = msg.getData<SceneEvent>();
            if (data.type == SceneEvent::TransitionComplete)
            {
                m_newHole = true;
            }
            else if (data.type == SceneEvent::PlayerIdle)
            {
                if (auto idx = m_playerIndices[m_currentClient][m_currentPlayer]; idx > -1)
                {
                    std::string emitterName = cro::Util::Random::value(0,1) == 1 ? "rough" : "bunker";
                    if (m_playerVoices[idx].hasEmitter(emitterName))
                    {
                        playAvatarSound(idx, emitterName, glm::vec3(0.f));
                    }
                }
            }
            else if (data.type == SceneEvent::PlayerBad)
            {
                playSound(AudioID::BadSport, glm::vec3(0.f), 0.8f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                playSoundDelayed(AudioID::NearMiss, glm::vec3(0.f), 1.f, 1.f, MixerChannel::Effects);
            }
            else if (data.type == SceneEvent::ChatMessage)
            {
                if (m_soundTimers[AudioID::Chat].elapsed() > ChatSoundTime)
                {
                    auto e = playSound(AudioID::Chat, glm::vec3(0.f), 0.3f);
                    e.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
                    float pitch = static_cast<float>(cro::Util::Random::value(7, 13)) / 10.f;
                    e.getComponent<cro::AudioEmitter>().setPitch(pitch);
                }
            }
            else if (data.type == SceneEvent::PlayerEliminated)
            {
                playSound(AudioID::NearMiss, glm::vec3(0.f), 1.f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
            }
            else if (data.type == SceneEvent::Poke)
            {
                playSound(AudioID::Poke, glm::vec3(0.f), 0.5f).getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Menu);
            }
        }
        break;
        }
    }
}

void GolfSoundDirector::process(float dt)
{
    SoundEffectsDirector::process(dt);

    if (m_crowdTimer.elapsed() > m_crowdTime)
    {
        if (m_crowdPositions)
        {
            const auto& cp = *m_crowdPositions;
            if (!cp.empty())
            {
                auto pos = glm::vec3((cp[cro::Util::Random::value(0u, cp.size() - 1)][3]));
                auto& emitter = playSound(AudioID::CrowdClearThroat + cro::Util::Random::value(0, 2), pos, 1.6f).getComponent<cro::AudioEmitter>();
                emitter.setMixerChannel(MixerChannel::Environment);
                emitter.setRolloff(0.45f);
            }
        }

        m_crowdTimer.restart();
        m_crowdTime = MinCrowdTime + cro::seconds(static_cast<float>(cro::Util::Random::value(-5, 25)));
    }
}

void GolfSoundDirector::addAudioScape(const std::string& path, cro::AudioResource& resource)
{
    //we emplace back even if it fails/has empty path so indices match the player indices.
    m_playerVoices.emplace_back().loadFromFile(path, resource);
}

void GolfSoundDirector::setPlayerIndex(std::size_t client, std::size_t player, std::int32_t index)
{
    CRO_ASSERT(index < m_playerVoices.size(), "load audioscapes first!");
    m_playerIndices[client][player] = index;
}

void GolfSoundDirector::setActivePlayer(std::size_t client, std::size_t player, bool skipAudio)
{
    m_currentClient = client;
    m_currentPlayer = player;
    
    if (m_newHole)
    {
        std::int32_t honour = (client << 8) | player;
        if (honour != m_honourID
            && client == m_sharedData.clientConnection.connectionID //only play if it's our client
            && !skipAudio) //don't play this if fast CPU as it overlaps other audio
        {
            playSoundDelayed(AudioID::Honour, glm::vec3(0.f), 1.f, 1.f, MixerChannel::Voice);
        }
        m_honourID = honour;
    }
    m_newHole = false;
}

//private
cro::Entity GolfSoundDirector::playSound(std::int32_t id, glm::vec3 position, float volume)
{
    const auto playDefault = [&, id, volume, position]()
    {
        auto ent = getNextEntity();
        ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[id]);
        ent.getComponent<cro::AudioEmitter>().setVolume(volume * 0.5f);
        ent.getComponent<cro::AudioEmitter>().setPitch(1.f);
        ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Voice);
        ent.getComponent<cro::AudioEmitter>().play();
        ent.getComponent<cro::Transform>().setPosition(position);
        return ent;
    };

    const auto playSpecial = [&, id, position]()
    {
        if (auto idx = m_playerIndices[m_currentClient][m_currentPlayer]; idx > -1)
        {
            std::string emitterName;
            switch (id)
            {
            default: break;
            case AudioID::TerrainBunker01:
            case AudioID::TerrainRough01:
            case AudioID::TerrainScrub01:
            case AudioID::TerrainWater01:

                {
                    auto r = cro::Util::Random::value(0, 3);
                    switch (r)
                    {
                    default:
                    case 0:
                        emitterName = "bunker";
                        break;
                    case 1:
                        emitterName = "rough";
                        break;
                    case 2:
                        emitterName = "scrub";
                        break;
                    case 3:
                        emitterName = "water";
                        break;
                    }
                }

                break;

            case AudioID::TerrainFairway01:
                emitterName = "fairway";
                break;
            case AudioID::TerrainGreen01:
                emitterName = "green";
                break;
            case AudioID::Hook:
                emitterName = "hook";
                break;
            case AudioID::Slice:
                emitterName = "slice";
                break;
            }

            if (m_playerVoices[idx].hasEmitter(emitterName))
            {
                return playAvatarSound(idx, emitterName, position);
            }
        }

        return playDefault();
    };

    switch (id)
    {
    default: 
        return playDefault();
    case AudioID::TerrainBunker01:
    case AudioID::TerrainFairway01:
    case AudioID::TerrainGreen01:
    case AudioID::TerrainRough01:
    case AudioID::TerrainScrub01:
    case AudioID::TerrainWater01:
    case AudioID::Hook:
    case AudioID::Slice:
        return playSpecial();
    }

    m_soundTimers[id].restart();
}

void GolfSoundDirector::playSoundDelayed(std::int32_t id, glm::vec3 position, float delay, float volume, std::uint8_t channel)
{
    auto entity = getScene().createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(delay);
    entity.getComponent<cro::Callback>().function =
        [&, id, position, volume, channel](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0)
        {
            playSound(id, position, volume).getComponent<cro::AudioEmitter>().setMixerChannel(channel);

            e.getComponent<cro::Callback>().active = false;
            getScene().destroyEntity(e);
        }
    };

    m_soundTimers[id].restart();
}

cro::Entity GolfSoundDirector::playAvatarSound(std::int32_t idx, const std::string& emitterName, glm::vec3 position)
{
    //stops repeating the same voice twice
    static std::int32_t lastIdx = 0;
    static std::string lastEmitter = emitterName;

    auto ent = getNextEntity();
    ent.getComponent<cro::AudioEmitter>() = m_playerVoices[idx].getEmitter(emitterName);
    ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Voice);
    ent.getComponent<cro::AudioEmitter>().setPitch(1.f);
    ent.getComponent<cro::AudioEmitter>().setVolume(0.5f);
    
    //urgh, we want to cancel this entirely really
    //but we need to return a valid entity...
    //if (idx != lastIdx || emitterName != lastEmitter)
    {
        ent.getComponent<cro::AudioEmitter>().play();
    }
    ent.getComponent<cro::Transform>().setPosition(position);


    lastIdx = idx;
    lastEmitter = emitterName;

    return ent;
}

void GolfSoundDirector::playAvatarSoundDelayed(std::int32_t idx, const std::string& emitterName, glm::vec3 position, float delay)
{
    if (m_playerVoices[idx].hasEmitter(emitterName))
    {
        auto entity = getScene().createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(delay);
        entity.getComponent<cro::Callback>().function =
            [&, idx, position, emitterName](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime -= dt;

            if (currTime < 0)
            {
                playAvatarSound(idx, emitterName, position);

                e.getComponent<cro::Callback>().active = false;
                getScene().destroyEntity(e);
            }
        };
    }
}

void GolfSoundDirector::applaud()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Crowd;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<VatAnimation>().applaud();
    };
    getScene().getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::Spectator;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().setUserData<bool>(true);
    };
    getScene().getSystem<cro::CommandSystem>()->sendCommand(cmd);
}