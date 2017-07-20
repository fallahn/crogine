/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/components/AudioSource.hpp>
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
    requireComponent<AudioSource>();
}

//public
void AudioSystem::process(cro::Time dt)
{
    //update the scene's listener details
    const auto& listener = getScene()->getActiveListener();
    AudioRenderer::setListenerVolume(listener.getComponent<AudioListener>().getVolume());
    const auto& tx = listener.getComponent<Transform>();
    auto worldPos = tx.getWorldPosition();
    AudioRenderer::setListenerPosition(worldPos);
    const auto& worldTx = tx.getWorldTransform();
    AudioRenderer::setListenerOrientation(Util::Matrix::getForwardVector(worldTx), Util::Matrix::getUpVector(worldTx));
    //AudioRenderer::setListenerOrientation({ 0.f ,0.f, -1.f }, { 0.f, 1.f, 0.f });
    DPRINT("Listener Position", std::to_string(worldPos.x) + ", " + std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z));

    //for each entity
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& audioSource = entity.getComponent<AudioSource>();
        
        //check its flags and update
        if (audioSource.m_newBuffer)
        {
            AudioRenderer::deleteAudioSource(audioSource.m_ID);
            audioSource.m_ID = AudioRenderer::requestAudioSource(audioSource.m_bufferID);
            audioSource.m_newBuffer = false;
        }

        if (audioSource.m_transportFlags & AudioSource::Play)
        {
            bool loop = (audioSource.m_transportFlags & AudioSource::Looped);
            AudioRenderer::playSource(audioSource.m_ID, loop);
        }
        else if (audioSource.m_transportFlags & AudioSource::Pause)
        {
            AudioRenderer::pauseSource(audioSource.m_ID);
        }
        else if (audioSource.m_transportFlags & AudioSource::Stop)
        {
            AudioRenderer::stopSource(audioSource.m_ID);
        }
        //reset all flags
        audioSource.m_transportFlags = 0;
        //set current state
        audioSource.m_state = static_cast<AudioSource::State>(AudioRenderer::getSourceState(audioSource.m_ID));
        //DPRINT("Audio State", (audioSource.m_state == AudioSource::State::Playing) ? "Playing" : "Stopped");

        //check its position and update
        if (entity.hasComponent<Transform>())
        {
            //set position
            auto worldPos = entity.getComponent<Transform>().getWorldPosition();
            AudioRenderer::setSourcePosition(audioSource.m_ID, worldPos);
            DPRINT("Sound Position", std::to_string(worldPos.x) + ", " + std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z));
        }

        //check properties such as pitch and gain
        AudioRenderer::setSourcePitch(audioSource.m_ID, audioSource.m_pitch);
        AudioRenderer::setSourceVolume(audioSource.m_ID, audioSource.m_volume);
        AudioRenderer::setSourceRolloff(audioSource.m_ID, audioSource.m_rolloff);
    }
}

//private
void AudioSystem::onEntityAdded(Entity entity)
{
    //check if buffer already added and summon new audio source
    auto& audioSource = entity.getComponent<AudioSource>();
    if(AudioRenderer::isValid() && audioSource.m_bufferID > 0)
    {
        if (audioSource.m_newBuffer)
        {
            CRO_ASSERT(audioSource.m_bufferID > 0, "Not a valid buffer ID");

            AudioRenderer::deleteAudioSource(audioSource.m_ID);
            audioSource.m_ID = AudioRenderer::requestAudioSource(audioSource.m_bufferID);
            audioSource.m_newBuffer = false;
        }
    }
    else
    {
        getEntities().pop_back(); //no entity for you!
    }
}