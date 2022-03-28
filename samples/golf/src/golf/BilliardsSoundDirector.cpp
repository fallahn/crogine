/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "BilliardsSoundDirector.hpp"
#include "BilliardsSystem.hpp"
#include "MessageIDs.hpp"
#include "GameConsts.hpp"

#include <crogine/audio/AudioResource.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Random.hpp>

BilliardsSoundDirector::BilliardsSoundDirector(cro::AudioResource& ar)
{
    static const std::array<std::string, AudioID::Count> FilePaths =
    {
        "assets/golf/sound/billiards/ball.wav",
        "assets/golf/sound/billiards/cushion.wav",
        "assets/golf/sound/billiards/pot.wav",
        "assets/golf/sound/billiards/cue.wav",
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
void BilliardsSoundDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BilliardsMessage)
    {
        const auto& data = msg.getData<BilliardBallEvent>();
        switch (data.type)
        {
        default: break;
        case BilliardBallEvent::Collision:
            switch (data.data)
            {
            default: break;
            case CollisionID::Cushion:
                playSound(AudioID::CushionHit, data.position);
                break;
            case CollisionID::Ball:
            {
                auto ent = playSound(AudioID::BallHit, data.position);
                ent.getComponent<cro::AudioEmitter>().setPitch(cro::Util::Random::value(0.95f, 1.05f));
            }
                break;
            case CollisionID::Pocket:
                playSound(AudioID::Pocket, data.position);
                break;
            }
            break;
        }
    }
    else if (msg.id == cro::Message::SkeletalAnimationMessage)
    {
        const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
        if (data.userType == 0)
        {
            playSound(AudioID::Cue, data.position);
        }
    }
}

//private
cro::Entity BilliardsSoundDirector::playSound(std::int32_t id, glm::vec3 position, float volume)
{
    const auto playDefault = [&, id, volume, position]()
    {
        auto ent = getNextEntity();
        ent.getComponent<cro::AudioEmitter>().setSource(*m_audioSources[id]);
        ent.getComponent<cro::AudioEmitter>().setVolume(volume);
        ent.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Voice);
        ent.getComponent<cro::AudioEmitter>().play();
        ent.getComponent<cro::Transform>().setPosition(position);
        return ent;
    };

    return playDefault();
}