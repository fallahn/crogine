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

#ifndef CRO_AUDIO_RENDERER_HPP_
#define CRO_AUDIO_RENDERER_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <glm/vec3.hpp>

#include <memory>
#include <string>

namespace cro
{
    /*!
    \brief Defines the interface for an audio renderer.
    Allows for definining multiple rendersystems for targetting different
    platforms. For example by default desktop platforms use an OpenAL renderer
    while mobile devices use SDL_mixer. To force a particular renderer define
    AL_AUDIO or SDL_AUDIO when building crogine.
    */
    class AudioRendererImpl
    {
    public:
        AudioRendererImpl() = default;
        ~AudioRendererImpl() = default;

        AudioRendererImpl(const AudioRendererImpl&) = delete;
        AudioRendererImpl(AudioRendererImpl&&) = delete;
        AudioRendererImpl& operator = (const AudioRendererImpl&) = delete;
        AudioRendererImpl& operator = (AudioRendererImpl&&) = delete;

        virtual bool init() = 0;        
        virtual void shutdown() = 0;

        virtual void setListenerPosition(glm::vec3) = 0;
        virtual void setListenerOrientation(glm::vec3, glm::vec3) = 0;
        virtual void setListenerVolume(float) = 0;

        virtual cro::int32 requestNewBuffer(const std::string&) = 0;
        virtual void deleteBuffer(cro::int32) = 0;

        virtual cro::int32 requestAudioSource(cro::int32) = 0;
        virtual void deleteAudioSource(cro::int32) = 0;

        virtual void playSource(cro::int32, bool) = 0;
        virtual void pauseSource(cro::int32) = 0;
        virtual void stopSource(cro::int32) = 0;

        virtual int32 getSourceState(cro::int32) const = 0;

        virtual void setSourcePosition(int32, glm::vec3) = 0;
        virtual void setSourcePitch(int32, float) = 0;
        virtual void setSourceVolume(int32, float) = 0;
        virtual void setSourceRolloff(int32, float) = 0;
    };


    /*!
    \brief AudioRenderer accessor class.
    Provides static access to the current audio renderer in a unified way.
    */
    class AudioRenderer final
    {
    public:
        /*!
        \brief Called by the main app at startup.
        This should be used to create any contexts or initialise audio
        hardware. Return true on success, else return false
        */
        static bool init();

        /*!
        \brief Used to tidy up any resources used by the implementation.
        This is called during shutdown
        */
        static void shutdown();

        /*!
        \brief Returns true if there is a valid AudioRenderer initialised
        */
        static bool isValid();

        /*!
        \brief Sets the listener position in the scene.
        \param position The listener position in world coordinates.
        Sounds are positioned and attenuated depending on where they
        are relative to the listener. Normally this is set to the
        postion of the active camera, but can be used to emulated effects
        such as remote microphones.
        */
        static void setListenerPosition(glm::vec3 position);

        /*!
        \brief sets the listener orientation.
        \param forward Vector representing the forward direction of the listener
        \param up A vector representing the up direction of the listener.
        This is generally only supported in the 3D renderers such as the OpenAL implementation.
        Otherwise setting this has no effect.
        */
        static void setListenerOrientation(glm::vec3 forward, glm::vec3 up);

        /*!
        \brief Sets the master volum of the listener.
        \param volume A positive value to set as the max gain of the
        listener. Negative values are automatically clamped to 0.
        */
        static void setListenerVolume(float volume);

        /*!
        \brief Requests a new buffer be created from the file at the given string.
        \returns A unique IDfor the new buffer on succes, else -1 if something failed.
        NOTE: there is no resource management done here, no checks are done to see if the
        file requested to be loaded already exists, and it is up to the requester to make
        sure buffers are properly released with deleteBuffer()
        */
        static cro::int32 requestNewBuffer(const std::string& path);

        /*!
        \brief Deletes a buffer with the given ID, freeing it from memory.
        All buffers retrieved via requestNewBuffer() must be deleted via this function.
        */
        static void deleteBuffer(cro::int32 buffer);

        /*!
        \brief Requests a new audio source bound to the given buffer.
        \param buffer ID of the buffer to which to bind the audio source.
        \returns A unique ID for the audio sourceon success else returns - 1
        NOTE these audio sources are not resource managed in anyway and is left
        entirely to the caller to make sucre that the audio source is properly
        deleted with deleteAudioSource() when it needs to be disposed
        */
        static cro::int32 requestAudioSource(cro::int32 buffer);

        /*!
        \brief Attempts to delete the audio source with the given ID.
        \param source ID of the audio source to delete.
        This MUST be called for all audio sources allocated with requestAudioSource()
        */
        static void deleteAudioSource(cro::int32 source);

        /*!
        \brief Attempts to play the given source.
        \param src ID of the audio source to play
        \param looped true to loop this source, else false
        */
        static void playSource(int32 src, bool looped);

        /*!
        \brief Pauses the given source if it is playing.
        \param src ID of the source to pause
        */
        static void pauseSource(int32 src);

        /*!
        \brief Stops the source if it is playing
        \param src ID of the source to stop
        */
        static void stopSource(int32 src);

        /*!
        \brief Returns the current state of the give source
        \returns 0 - Playing, 1 - Paused, 2 - Stopped
        */
        static int32 getSourceState(int32 src);

        /*!
        \brief Sets the position of the given source
        \param source Source ID of which to set the position
        \param position Vector3 containing the world position of the source
        */
        static void setSourcePosition(int32 source, glm::vec3 position);

        /*!
        \brief Sets the given source pitch
        */
        static void setSourcePitch(int32 src, float pitch);

        /*!
        \brief Sets the given source volume
        */
        static void setSourceVolume(int32 src, float vol);

        /*!
        \brief Sets the give source rolloff
        */
        static void setSourceRolloff(int32 src, float rolloff);

    private:
        static std::unique_ptr<AudioRendererImpl> m_impl;
    };
}

#endif