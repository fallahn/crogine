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

#include <crogine/audio/sound_system/SoundRecorder.hpp>
#include <crogine/gui/Gui.hpp>

#include "../ALCheck.hpp"
#include "../AudioRenderer.hpp"

#ifdef __APPLE__
#include "al.h"
#include "alc.h"

//silence deprecated openal warnings
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <cstring>

#define RECORDING_DEVICE static_cast<ALCdevice*>(m_recordingDevice)


using namespace cro;

namespace
{
    constexpr std::array OPUS_SAMPLE_RATES =
    {
        8000, 12000, 16000, 24000, 48000
    };

    constexpr std::uint32_t OPUS_MAX_PACKET_SIZE = 1276 * 3;
    constexpr std::uint32_t OPUS_BITRATE = 64000;

    const std::string MissingDevice("No Device Active");
    const std::string DefaultDevice("Default");

#ifdef CRO_DEBUG_
    std::int32_t pcmBufferCaptureSize = 0;

#endif
}

SoundRecorder::SoundRecorder()
    : m_deviceIndex     (-1),
    m_recordingDevice   (nullptr),
    m_active            (false),
    m_channelCount      (0),
    m_sampleRate        (0)
{
    enumerateDevices();

#ifdef CRO_DEBUG_
    /*registerWindow([&]() 
        {
            if (ImGui::Begin("Sound Recorder Debug"))
            {
                ImGui::Text("PCM Buffer Size %d", pcmBufferCaptureSize);
            }
            ImGui::End();
        });*/
#endif
}

SoundRecorder::~SoundRecorder()
{
    closeDevice();
}

//public
void SoundRecorder::refreshDeviceList()
{
    m_deviceList.clear();
    enumerateDevices();
}

const std::vector<std::string>& SoundRecorder::listDevices() const
{
    return m_deviceList;
}

const std::string& SoundRecorder::getActiveDevice() const
{
    if (m_recordingDevice)
    {
        return m_deviceIndex == -1 ? DefaultDevice : m_deviceList[m_deviceIndex];
    }

    return MissingDevice;
}

bool SoundRecorder::openDevice(const std::string& device, std::int32_t channelCount, std::int32_t sampleRate, bool createEncoder)
{
    CRO_ASSERT(channelCount == 1 || channelCount == 2, "Only mono and stereo are supported");
    m_channelCount = channelCount;
    m_sampleRate = sampleRate;
    m_pcmBuffer.resize((sampleRate / 25));

    if (!AudioRenderer::isValid())
    {
        LogE << "SoundRecorder::openDevice(): No valid audio renderer available." << std::endl;
        return false;
    }

    closeDevice();

    if (auto result = std::find(m_deviceList.begin(), m_deviceList.end(), device); result != m_deviceList.end())
    {
        m_deviceIndex = static_cast<std::int32_t>(std::distance(m_deviceList.begin(), result));
    }
    else
    {
        m_deviceIndex = -1;
    }


    if (createEncoder &&
        std::find(OPUS_SAMPLE_RATES.begin(), OPUS_SAMPLE_RATES.end(), sampleRate) != OPUS_SAMPLE_RATES.end())
    {
        //create the opus encoder and decoder
        Opus::Context ctx;
        ctx.channelCount = channelCount;
        ctx.sampleRate = sampleRate;
        ctx.bitrate = OPUS_BITRATE;
        ctx.maxPacketSize = OPUS_MAX_PACKET_SIZE;

        m_encoder = std::make_unique<Opus>(ctx);
    }

    return openSelectedDevice();
}

void SoundRecorder::closeDevice()
{
    if (m_recordingDevice)
    {
        alcCaptureStop(RECORDING_DEVICE);
        alcCaptureCloseDevice(RECORDING_DEVICE);
    }
    m_active = false;
    m_recordingDevice = nullptr;
    m_encoder.reset();
}

bool SoundRecorder::isActive() const
{
    return m_active;
}

void SoundRecorder::getEncodedPacket(std::vector<std::uint8_t>& dst) const
{
    dst.clear();

    if (m_encoder)
    {
        std::int32_t pcmCount = 0;
        if (const auto* data = getPCMData(&pcmCount); data && pcmCount)
        {
            m_encoder->encode(data, pcmCount, dst);
        }
    }
}

const std::int16_t* SoundRecorder::getPCMData(std::int32_t* count) const
{
    if (m_recordingDevice)
    {
        ALCint sampleCount = 0;
        alcGetIntegerv(RECORDING_DEVICE, ALC_CAPTURE_SAMPLES, 1, &sampleCount);

        if (sampleCount >= m_pcmBuffer.size())
        {
            alcCaptureSamples(RECORDING_DEVICE, m_pcmBuffer.data(), m_pcmBuffer.size());

            *count = static_cast<std::int32_t>(m_pcmBuffer.size());
            return m_pcmBuffer.data();
        }
    }

    *count = 0;
    return nullptr;
}

std::int32_t SoundRecorder::getChannelCount() const
{
    return m_channelCount;
}

std::int32_t SoundRecorder::getSampleRate() const
{
    return m_sampleRate;
}


//private
void SoundRecorder::enumerateDevices()
{
    if (AudioRenderer::isValid())
    {
        const ALchar* deviceList = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
        auto* next = deviceList + 1;
        std::size_t len = 0;

        while (deviceList && *deviceList != '\0'
            && next
            && *next != '\0')
        {
            m_deviceList.push_back(std::string(deviceList));
            len = strlen(deviceList);
            deviceList += (len + 1);
            next += (len + 2);
        }
        
        /*const auto* defaultDevice = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
        if (defaultDevice)
        {
            m_deviceList.push_back(std::string(defaultDevice));
        }*/

        return;
    }

    m_deviceList.push_back("No Audio Device Available");
}

bool SoundRecorder::openSelectedDevice()
{
    //TODO hmmm we appear to get a crash/heap corruption when setting this to stereo?
    if (!m_recordingDevice)
    {
        const auto format = m_channelCount == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        if (m_deviceIndex > -1)
        {
            m_recordingDevice = alcCaptureOpenDevice(m_deviceList[m_deviceIndex].c_str(), m_sampleRate, format, m_pcmBuffer.size() * 3);
            LogI << "Trying to open " << m_deviceList[m_deviceIndex].c_str() << std::endl;
        }
        else
        {
            m_recordingDevice = alcCaptureOpenDevice(nullptr, m_sampleRate, format, m_pcmBuffer.size() * 3);
            LogI << "Trying to open default recording device" << std::endl;
        }

        if (!m_recordingDevice)
        {
            LogE << "Failed opening device for recording" << std::endl;
        }
        else
        {
            LogI << "Opened device for recording" << std::endl;
            m_active = true;
            alcCaptureStart(RECORDING_DEVICE);
        }
    }
    return m_recordingDevice != nullptr;
}