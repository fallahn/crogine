/*-----------------------------------------------------------------------

Matt Marchant 2024
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
#include <crogine/audio/sound_system/OpusEncoder.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <vector>
#include <string>
#include <memory>

namespace cro
{
    /*!
    \brief The sound recorder captures audio from the specified audio device
    and optionally creates an opus encoder for streaming audio
    */
    class CRO_EXPORT_API SoundRecorder
#ifdef CRO_DEBUG_
        : public cro::GuiClient
#endif
    {
    public:
        SoundRecorder();
        ~SoundRecorder();

        SoundRecorder(const SoundRecorder&) = delete;
        SoundRecorder(SoundRecorder&&) = delete;

        SoundRecorder& operator = (const SoundRecorder&) = delete;
        SoundRecorder& operator = (SoundRecorder&&) = delete;

        /*!
        \brief Refreshes the list of devices
        */
        void refreshDeviceList();
        
        /*!
        \brief Lists available recording devices
        */
        const std::vector<std::string>& listDevices() const;

        /*!
        \brief Returns the name of the active device if it is open
        */
        const std::string& getActiveDevice() const;

        /*!
        \brief Attempts to open the device with the given name and starts recording.
        \param device Name of the recording device to attempt to open
        \param channels Number of channels to attempt to capture. Must be 1 (mono) or 2 (stereo)
        \param sampleRate The sample rate at which to capture. To enable opus encoding this must be
        8000, 12000, 16000, 24000 or 48000. Other sample rates can be used, but opus encoding will
        be unavailable, even if createEncoder was true
        \param createEncoder If true this will attempt to create an opus packet encoder to use with
        getEncodedPacket(). However this will be ignored is sampleRate is not a compatible value.
        Use listDevices() to obtain a list of valid name strings
        */
        bool openDevice(const std::string& device, std::int32_t channels, std::int32_t sampleRate, bool createEncoder = false);

        /*!
        \brief Closes the current recording device or does nothing if no device is open
        */
        void closeDevice();

        /*!
        \brief Returns true if a device is open and capturing else
        returns false
        */
        bool isActive() const;

        /*!
        \brief Resizes and fills the given buffer with an opus encoded packet.

        This is only available if the recording device was opened with a compatible
        sample rate and creteEncoder set to true. Otherwise this function does nothing.

        NOTE that this internally calls getPCMData() which will drain the
        audio buffer, so you should either use just this function or
        getPCMData() BUT NOT BOTH.
        */
        void getEncodedPacket(std::vector<std::uint8_t>& dst) const;

        /*!
        \brief Returns a pointer to the raw captured PCM data (if any)
        \param count Pointer to an int32_t which is filled with the
        number of samples in the buffer.
        NOTE that this is internally called by getEncodedPacket() if a
        valid opus encoder is available which will drain the audio buffer,
        so you should either use just this function or getEncodedPackets()
        BUT NOT BOTH.
        */
        const std::int16_t* getPCMData(std::int32_t* count) const;

        /*!
        \brief Returns a pointer to the raw captured PCM data (if any)
        \param count Pointer to an int32_t which is filled with the
        number of samples in the buffer.

        Audio is captured as float by default, calling this with int16
        or uint8 will convert it on the fly.

        NOTE that this is internally called by getEncodedPacket() if a
        valid opus encoder is available which will drain the audio buffer,
        so you should either use just this function or getEncodedPackets()
        BUT NOT BOTH.
        */
        template <typename T>
        const T* getPCM(std::int32_t* count) const 
        {
            static_assert(sizeof(T) != sizeof(T), "Use template specialisation for float, int16 or uint8");
            return nullptr;
        }

        /*!
        \brief Returns the number of audio channels with which the audio
        will be captured
        */
        std::int32_t getChannelCount() const;

        /*!
        \brief Returns the samplerate at which the audio will be captured
        */
        std::int32_t getSampleRate() const;


    private:

        std::vector<std::string> m_deviceList;
        std::int32_t m_deviceIndex;

        void* m_recordingDevice;
        bool m_active;

        std::int32_t m_channelCount;
        std::int32_t m_sampleRate;

        mutable std::vector<std::int16_t> m_pcmBuffer;
        std::unique_ptr<Opus> m_encoder;

        std::vector<float> m_captureBuffer;
        std::vector<std::int16_t> m_shortConversionBuffer;
        std::vector<std::uint8_t> m_byteConversionBuffer;
        void updateCaptureBuffer();

        void enumerateDevices();
        bool openSelectedDevice();
    };

    template <>
    inline const float* SoundRecorder::getPCM(std::int32_t* count) const
    {
        return nullptr;
    }

    template<>
    inline const std::int16_t* SoundRecorder::getPCM(std::int32_t* count) const
    {
        return nullptr;
    }

    template<>
    inline const std::uint8_t* SoundRecorder::getPCM(std::int32_t* count) const
    {
        return nullptr;
    }
}