/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/detail/glm/vec3.hpp>

#include <memory>
#include <string>

namespace cro
{
    namespace Detail
    {
        struct PCMData;
    }
    
    /*!
    \brief Defines the interface for an audio renderer.
    Allows for defining multiple rendersystems for targeting different
    platforms. Currently only OpenAL is implemented as a renderer.
    */
    class AudioRendererImpl
    {
    public:
        AudioRendererImpl() = default;
        virtual ~AudioRendererImpl() = default;

        AudioRendererImpl(const AudioRendererImpl&) = delete;
        AudioRendererImpl(AudioRendererImpl&&) = delete;
        AudioRendererImpl& operator = (const AudioRendererImpl&) = delete;
        AudioRendererImpl& operator = (AudioRendererImpl&&) = delete;

        virtual bool init() = 0;        
        virtual void shutdown() = 0;

        virtual void setListenerPosition(glm::vec3) = 0;
        virtual void setListenerOrientation(glm::vec3, glm::vec3) = 0;
        virtual void setListenerVolume(float) = 0;
        virtual void setListenerVelocity(glm::vec3) = 0;

        virtual glm::vec3 getListenerPosition() const = 0;

        virtual std::int32_t requestNewBuffer(const std::string&) = 0;
        virtual std::int32_t requestNewBuffer(const Detail::PCMData&) = 0;
        virtual void deleteBuffer(std::int32_t) = 0;

        virtual std::int32_t requestNewStream(const std::string&) = 0;
        virtual void deleteStream(std::int32_t) = 0;

        //note that this is the openAL source, not the buffer or stream behind it
        virtual std::int32_t requestAudioSource(std::int32_t, bool) = 0;
        virtual void updateAudioSource(std::int32_t, std::int32_t, bool) = 0;
        virtual void deleteAudioSource(std::int32_t) = 0;

        virtual void playSource(std::int32_t, bool) = 0;
        virtual void pauseSource(std::int32_t) = 0;
        virtual void stopSource(std::int32_t) = 0;
        virtual void setPlayingOffset(std::int32_t, cro::Time) = 0;

        virtual std::int32_t getSourceState(std::int32_t) const = 0;

        virtual void setSourcePosition(std::int32_t, glm::vec3) = 0;
        virtual void setSourcePitch(std::int32_t, float) = 0;
        virtual void setSourceVolume(std::int32_t, float) = 0;
        virtual void setSourceRolloff(std::int32_t, float) = 0;
        virtual void setSourceVelocity(std::int32_t, glm::vec3) = 0;
        virtual void setDopplerFactor(float) = 0;
        virtual void setSpeedOfSound(float) = 0;

        //optionally override this to implement ImGui debug printing
        //*without* window begin/end
        virtual void printDebug() {}
    };


    /*!
    \brief AudioRenderer accessor class.
    Provides static access to the current audio renderer in a unified way.

    Note that, while these functions can be accessed anywhere from within
    crogine, an AudioSystem active in a Scene will call these automatically
    to update any active AudioEmitters and the Scene's listener. Depending
    on the order in which they are used manually calling these functions may
    overwrite the output of an AudioSystem or vice versa.
    */
    class AudioRenderer final
    {
    public:
        /*!
        \brief Called by the main app at startup.
        This should be used to create any contexts or initialise audio
        hardware. Returns true on success, else return false
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
        \brief Sets the master volume of the listener.
        \param volume A positive value to set as the max gain of the
        listener. Negative values are automatically clamped to 0.
        */
        static void setListenerVolume(float volume);

        /*!
        \brief Sets the velocity value of the Listener.
        The velocity is used by the AudioRenderer to calculate Doppler effects.
        \param velocity A vector 3 representing the current velocity of the Listener.
        By default this is in metres per second.
        */
        static void setListenerVelocity(glm::vec3 velocity);

        /*!
        \brief Returns the current position of the listener in world units
        */
        static glm::vec3 getListenerPosition();

        /*!
        \brief Requests a new buffer be created from the file at the given string.
        \returns A unique ID for the new buffer on success, else -1 if something failed.
        NOTE: there is no resource management done here, no checks are done to see if the
        file requested to be loaded already exists, and it is up to the requester to make
        sure buffers are properly released with deleteBuffer()
        */
        static std::int32_t requestNewBuffer(const std::string& path);

        /*!
        \brief Requests a new buffer from the given PCMData struct.
        */
        static std::int32_t requestNewBuffer(const Detail::PCMData&);

        /*!
        \brief Deletes a buffer with the given ID, freeing it from memory.
        All buffers retrieved via requestNewBuffer() must be deleted via this function.
        */
        static void deleteBuffer(std::int32_t buffer);

        /*!
        \brief Requests a new audio stream from a file on disk.
        \param path Path to file to stream.
        \returns ID of stream, or -1 if opening the file failed
        */
        static std::int32_t requestNewStream(const std::string& path);

        /*!
        \brief Deletes the stream with the given ID, if it exists
        */
        static void deleteStream(std::int32_t);

        /*!
        \brief Requests a new audio source bound to the given buffer.
        \param buffer ID of the buffer to which to bind the audio source.
        \param streaming Should be true if the buffer ID is associated with a
        an AudioStream data source, else false for an AudioBuffer.
        \returns A unique ID for the audio source on success else returns - 1
        NOTE these audio sources are not resource managed in anyway and is left
        entirely to the caller to make sure that the audio source is properly
        deleted with deleteAudioSource() when it needs to be disposed
        */
        static std::int32_t requestAudioSource(std::int32_t buffer, bool streaming);

        /*!
        \brief Updates the given audio source with a new buffer.
        \param sourceID ID of the audio source to update
        \param bufferID ID of the buffer to assign to the source
        \param streaming Should be true if the buffer ID is associated with a
        an AudioStream data source, else false for an AudioBuffer.
        */
        static void updateAudioSource(std::int32_t sourceID, std::int32_t bufferID, bool streaming);

        /*!
        \brief Attempts to delete the audio source with the given ID.
        \param source ID of the audio source to delete.
        This MUST be called for all audio sources allocated with requestSourceEmitter()
        */
        static void deleteAudioSource(std::int32_t source);

        /*!
        \brief Attempts to play the given source.
        \param src ID of the audio source to play
        \param looped true to loop this source, else false
        */
        static void playSource(std::int32_t src, bool looped);

        /*!
        \brief Pauses the given source if it is playing.
        \param src ID of the source to pause
        */
        static void pauseSource(std::int32_t src);

        /*!
        \brief Stops the source if it is playing
        \param src ID of the source to stop
        */
        static void stopSource(std::int32_t src);

        /*!
        \brief Sets the offset of the source to the
        given amount of time from the beginning.
        This has no effect if the source is stopped, only
        if it is playing or paused. If the time exceeds the
        length of the source, then playback is automatically
        stopped.
        */
        static void setPlayingOffset(std::int32_t src, cro::Time offset);

        /*!
        \brief Returns the current state of the give source
        \returns 0 - Playing, 1 - Paused, 2 - Stopped
        */
        static std::int32_t getSourceState(std::int32_t src);

        /*!
        \brief Sets the position of the given source
        \param source Source ID of which to set the position
        \param position Vector3 containing the world position of the source
        */
        static void setSourcePosition(std::int32_t source, glm::vec3 position);

        /*!
        \brief Sets the given source pitch
        */
        static void setSourcePitch(std::int32_t src, float pitch);

        /*!
        \brief Sets the given source volume
        */
        static void setSourceVolume(std::int32_t src, float vol);

        /*!
        \brief Sets the given source roll-off
        */
        static void setSourceRolloff(std::int32_t src, float rolloff);

        /*
        \brief Sets the given source velocity
        */
        static void setSourceVelocity(std::int32_t src, glm::vec3 velocity);

        /*!
        \brief Sets the Doppler effect multiplier.
        This has the effects of multiplying the current source and listener velocities
        for an increased or more subtle Doppler effect. Must be a positive value.
        Default value is 1
        \param multiplier Effect multiplier value
        */
        static void setDopplerFactor(float multiplier);

        /*!
        \brief Sets the perceived speed of sound.
        This is used when calculating pitch change during Doppler effects. By 
        default this value is 343.3 which is the approximate metres per second
        sound travels through air. Setting this to a lower value will simulate
        sound as it travels through water, for example
        Must be greater than zero.
        \param speed Speed at which sound is to be simulated in world units
        */
        static void setSpeedOfSound(float speed);

        /*!
        \brief Prints any debug info available to the current ImGui window
        EG call this within your own window begin/end
        */
        static void printDebug();


        /*!
        \brief Returns a pointer to the active implementation cast to type T
        */
        template <typename T>
        static T* getImpl()
        {
            static_assert(std::is_base_of<AudioRendererImpl, T>);
            CRO_ASSERT(m_impl, "Not initialised!");
            return dynamic_cast<T*> (m_impl.get());
        }

    private:
        static std::unique_ptr<AudioRendererImpl> m_impl;
    };
}