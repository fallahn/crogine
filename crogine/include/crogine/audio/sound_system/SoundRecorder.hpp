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
#include <crogine/gui/GuiClient.hpp>

#include <vector>
#include <string>

namespace cro
{
    /*!
    \brief The sound recorder captures audio from the specified audio device
    and encodes it to a stream of Opus packets.
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
        \brief Attempts to open the device with the given name for recording.
        Use listDevices() to obtain a list of valid name strings
        */
        bool openDevice(const std::string& device /*TODO set sample rate and channel count*/);

        /*!
        \brief Closes the current recording device or does nthing if no device is open
        */
        void closeDevice();

        /*!
        \brief Starts recording with the currently open device, or attempts to
        open the last used device (or default device if no device was specified)
        and starts recording.
        \returns true if recording started or false if no device was available.
        */
        bool start();

        /*!
        \brief Stops recording if recording is active, else does nothing
        */
        void stop();

        /*!
        \brief Returns true if a device is open and capturing else
        returns false
        */
        bool isActive() const;

        /*!
        \brief Resizes and fills the given buffer with opus encoded packets.

        NOTE that this internally calls getPCMData() which will drain the
        audio buffer, so you should either use just this function or
        getPCMData() BUT NOT BOTH.
        */
        void getEncodedPackets(std::vector<std::uint8_t>& dst) const;

        /*!
        \brief Returns a pointer to the raw captured PCM data (if any)
        \param count Pointer to an int32_t which is filled with the
        number of samples in the buffer.
        NOTE that this is internally called by getEncodedPackets()
        which will drain the audio buffer, so you should either use just
        this function or getEncodedPackets() BUT NOT BOTH.
        */
        const std::int16_t* getPCMData(std::int32_t* count) const;

        /*!
        \brief Returns the number of audio channels with which the audio
        will be captured
        */
        constexpr std::int32_t getChannelCount() const;

        /*!
        \brief Returns the samplerate at which the audio will be captured
        */
        constexpr std::int32_t getSampleRate() const;


        /*!
        TODO this doesn't really belong here per se, for now we're using it
        to make sure packet decoding works correctly
        */
        std::vector<std::int16_t> decodePacket(const std::vector<std::uint8_t>&) const;


    private:

        std::vector<std::string> m_deviceList;
        std::int32_t m_deviceIndex;

        void* m_recordingDevice;
        bool m_active;

        void* m_encoder;
        void* m_decoder;

        //buffer for PCM captured from device
        mutable std::vector<std::int16_t> m_pcmBuffer;
        mutable std::vector<std::int16_t> m_pcmDoubleBuffer;
        mutable std::uint32_t m_pcmBufferOffset;
        mutable bool m_pcmBufferReady;

        mutable std::vector<std::int16_t> m_opusInBuffer;
        mutable std::vector<std::uint8_t> m_opusOutBuffer;

        void enumerateDevices();
        bool openSelectedDevice();
    };
}