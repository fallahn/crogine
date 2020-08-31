/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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
#include "../../audio/AudioRenderer.hpp"

#include <crogine/audio/AudioMixer.hpp>

#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/util/Matrix.hpp>

using namespace cro;

AudioSystem::AudioSystem(MessageBus& mb)
    : System(mb, typeid(AudioSystem))
{
    requireComponent<AudioEmitter>();
}

//public
void AudioSystem::process(float)
{
    //update the scene's listener details
    const auto& listener = getScene()->getActiveListener();
    AudioRenderer::setListenerVolume(listener.getComponent<AudioListener>().getVolume() * AudioMixer::m_masterVol);
    const auto& tx = listener.getComponent<Transform>();
    auto worldPos = tx.getWorldPosition();
    AudioRenderer::setListenerPosition(worldPos);
    const auto& worldTx = tx.getWorldTransform();
    AudioRenderer::setListenerOrientation(Util::Matrix::getForwardVector(worldTx), Util::Matrix::getUpVector(worldTx));
    //DPRINT("Listener Position", std::to_string(worldPos.x) + ", " + std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z));

    //for each entity
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& audioSource = entity.getComponent<AudioEmitter>();
        
        if (audioSource.m_dataSourceID < 0) //we don't have a valid buffer yet (remember streams are 0 based)
        {
            continue;
        }

        //check its flags and update
        if (audioSource.m_newDataSource)
        {
            if (audioSource.m_ID < 1)
            {
                //request a new source if this emitter is not yet used
                audioSource.m_ID = AudioRenderer::requestAudioSource(audioSource.m_dataSourceID, (audioSource.m_sourceType == AudioSource::Type::Stream));
            }
            else
            {
                //update the existing source
                AudioRenderer::updateAudioSource(audioSource.m_ID, audioSource.m_dataSourceID, (audioSource.m_sourceType == AudioSource::Type::Stream));
            }
            audioSource.m_newDataSource = false;
        }

        if ((audioSource.m_transportFlags & AudioEmitter::Play)
            && audioSource.m_state != AudioEmitter::State::Playing)
        {
            bool loop = (audioSource.m_transportFlags & AudioEmitter::Looped);
            AudioRenderer::playSource(audioSource.m_ID, loop);
        }
        else if (audioSource.m_transportFlags & AudioEmitter::Pause)
        {
            AudioRenderer::pauseSource(audioSource.m_ID);
        }
        else if (audioSource.m_transportFlags & AudioEmitter::Stop)
        {
            AudioRenderer::stopSource(audioSource.m_ID);
        }
        //reset all flags, but preserve Loop flag
        audioSource.m_transportFlags &= AudioEmitter::Looped;
        //set current state
        audioSource.m_state = static_cast<AudioEmitter::State>(AudioRenderer::getSourceState(audioSource.m_ID));
        //DPRINT("Audio State", (audioSource.m_state == AudioEmitter::State::Playing) ? "Playing" : "Stopped");

        //check its position and update
        if (entity.hasComponent<Transform>())
        {
            //set position
            auto pos = entity.getComponent<Transform>().getWorldPosition();
            AudioRenderer::setSourcePosition(audioSource.m_ID, pos);
            //DPRINT("Sound Position", std::to_string(worldPos.x) + ", " + std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z));
        }

        //check properties such as pitch and gain
        AudioRenderer::setSourcePitch(audioSource.m_ID, audioSource.m_pitch);
        AudioRenderer::setSourceVolume(audioSource.m_ID, audioSource.m_volume * AudioMixer::m_channels[audioSource.m_mixerChannel] * AudioMixer::m_prefadeChannels[audioSource.m_mixerChannel]);
        AudioRenderer::setSourceRolloff(audioSource.m_ID, audioSource.m_rolloff);

        //if we're steaming make sure to update the stream buffer
        if (audioSource.m_sourceType == AudioSource::Type::Stream)
        {
            AudioRenderer::updateStream(audioSource.m_dataSourceID);
        }
    }
}

//private
void AudioSystem::onEntityAdded(Entity entity)
{
    //check if buffer already added and summon new audio source
    auto& audioSource = entity.getComponent<AudioEmitter>();
    if(AudioRenderer::isValid())
    {
        if (audioSource.m_newDataSource)
        {
            if (audioSource.m_ID < 1)
            {
                //request a new source if this emiter is not yet used
                audioSource.m_ID = AudioRenderer::requestAudioSource(audioSource.m_dataSourceID, (audioSource.m_sourceType == AudioSource::Type::Stream));
            }
            audioSource.m_newDataSource = false;
        }
    }
    else
    {
        getEntities().pop_back(); //no entity for you!
    }
}