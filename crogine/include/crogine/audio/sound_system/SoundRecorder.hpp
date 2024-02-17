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
        bool openDevice(const std::string& device /*TODO set sample rate  and channel count*/);

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
        \brief Returns the buffer of currently encoded opus packets
        \param count Pointer to an int32_t which will be filled with the 
        *number of packets* (not bytes) contained in the buffer.
        TODO state packet size etc
        */
        const std::uint8_t* getEncodedPackets(std::int32_t* count) const;

    private:

        std::vector<std::string> m_deviceList;
        std::int32_t m_deviceIndex;

        void* m_recordingDevice;
        bool m_active;


        //buffer for PCM captured from device
        mutable std::vector<std::uint8_t> m_pcmBuffer;


        void enumerateDevices();
        bool openSelectedDevice();
    };
}