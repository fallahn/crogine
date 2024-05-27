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

#include <opus.h>

#include <cstring>

#define RECORDING_DEVICE static_cast<ALCdevice*>(m_recordingDevice)
#define OPUS_ENCODER static_cast<OpusEncoder*>(m_encoder)
#define OPUS_DECODER static_cast<OpusDecoder*>(m_decoder)

using namespace cro;

namespace
{
    //testing shows the capture buffer is directly related to sample rate, eg 480 samples (@60hz) for 24KHz sample rate
    constexpr std::uint32_t CHANNEL_COUNT = 1;
    constexpr std::uint32_t SAMPLE_RATE = 24000; //hmm opus doesn't support rates which aren't a multiple of 8000
    constexpr std::uint32_t SAMPLE_SIZE = sizeof(std::uint16_t);

    constexpr std::uint32_t OPUS_FRAME_SIZE = SAMPLE_RATE / 25; //to make sure the buffer aligns with a whole frame this needs to be a multiple of the capture rate
    constexpr std::uint32_t OPUS_MAX_FRAME_SIZE = OPUS_FRAME_SIZE * 3;
    constexpr std::uint32_t OPUS_MAX_PACKET_SIZE = 1276 * 3;
    constexpr std::uint32_t OPUS_BITRATE = 64000;

    constexpr ALCsizei RECORD_BUFFER_SIZE = OPUS_MAX_FRAME_SIZE;

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
    m_encoder           (nullptr),
    m_decoder           (nullptr),
    m_pcmBuffer         (OPUS_FRAME_SIZE),
    m_opusInBuffer      (OPUS_FRAME_SIZE),
    m_opusOutBuffer     (OPUS_MAX_PACKET_SIZE)
{
    enumerateDevices();

    std::int32_t err = 0;
    m_encoder = opus_encoder_create(SAMPLE_RATE, CHANNEL_COUNT, OPUS_APPLICATION_VOIP, &err);

    if (err < 0)
    {
        LogE << "Unable to create Opus encoder, err: " << opus_strerror(err) << std::endl;
        m_encoder = nullptr;
    }
    else
    {
        err = opus_encoder_ctl(OPUS_ENCODER, OPUS_SET_BITRATE(OPUS_BITRATE));
        if (err < 0)
        {
            LogE << "Failed to set encoder bitrate to " << OPUS_BITRATE << ": " << opus_strerror(err) << std::endl;
        }
    }

    //TODO move this to a util somewhere
    m_decoder = opus_decoder_create(SAMPLE_RATE, CHANNEL_COUNT, &err);
    if (err < 0)
    {
        LogE << "Unable to create Opus decoder, err: " << opus_strerror(err) << std::endl;
        m_decoder = nullptr;
    }

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
    //stop();
    closeDevice();

    if (m_encoder)
    {
        opus_encoder_destroy(OPUS_ENCODER);
    }

    if (m_decoder)
    {
        opus_decoder_destroy(OPUS_DECODER);
    }
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

bool SoundRecorder::openDevice(const std::string& device)
{
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
}

bool SoundRecorder::isActive() const
{
    return m_active;
}

void SoundRecorder::getEncodedPackets(std::vector<std::uint8_t>& dst) const
{
    dst.clear();

    if (!m_encoder)
    {
        return;
    }

    std::int32_t pcmCount = 0;
    if (const auto* data = getPCMData(&pcmCount); data && pcmCount)
    {
        //TODO the inital buffer size can be greater than OPUS_FRAME_SIZE
        //so we want to loop over the incoming data in frame size chunks (or less)

        m_opusInBuffer.resize(pcmCount);

        //opus uses big endian data (BUT *DOES* IT???)
        for (auto i = 0u; i < CHANNEL_COUNT * pcmCount; ++i)
        {
            m_opusInBuffer[i] = data[i];// (data[sizeof(std::int16_t) * i + 1] << 8) | data[sizeof(std::int16_t) * i];
        }

        auto byteCount = opus_encode(OPUS_ENCODER, m_opusInBuffer.data(), pcmCount, m_opusOutBuffer.data(), OPUS_MAX_PACKET_SIZE);
        if (byteCount > 0)
        {
            dst.resize(byteCount);
            std::memcpy(dst.data(), m_opusOutBuffer.data(), byteCount);
        }
        else
        {
            LogI << opus_strerror(byteCount) << std::endl;
        }
    }
    
    return;
}

const std::int16_t* SoundRecorder::getPCMData(std::int32_t* count) const
{
    if (m_recordingDevice)
    {
        ALCint sampleCount = 0;
        alcGetIntegerv(RECORDING_DEVICE, ALC_CAPTURE_SAMPLES, 1, &sampleCount);

        if (sampleCount >= OPUS_FRAME_SIZE)
        {
            alcCaptureSamples(RECORDING_DEVICE, m_pcmBuffer.data(), OPUS_FRAME_SIZE);

            *count = OPUS_FRAME_SIZE;
            return m_pcmBuffer.data();
        }
    }

    *count = 0;
    return nullptr;
}

constexpr std::int32_t SoundRecorder::getChannelCount() const
{
    //ugh why cast WHHHYYY
    return static_cast<std::int32_t>(CHANNEL_COUNT);
}

constexpr std::int32_t SoundRecorder::getSampleRate() const
{
    return static_cast<std::int32_t>(SAMPLE_RATE);
}

std::vector<std::int16_t> SoundRecorder::decodePacket(const std::vector<std::uint8_t>& packet) const
{
    std::vector<std::int16_t> retVal;

    static std::vector<std::int16_t> decodeBuffer(OPUS_MAX_FRAME_SIZE);

    if (m_decoder)
    {
        auto frameSize = opus_decode(OPUS_DECODER, packet.data(), packet.size(), decodeBuffer.data(), decodeBuffer.size(), 0);
        if (frameSize > 0)
        {
            std::vector<std::uint8_t> bytesBuffer(OPUS_MAX_FRAME_SIZE * CHANNEL_COUNT * 2);

            for (auto i = 0u; i < CHANNEL_COUNT * frameSize; ++i)
            {
                bytesBuffer[2 * i] = decodeBuffer[i] & 0xff;
                bytesBuffer[2 * i + 1] = (decodeBuffer[i] >> 8) & 0xff;
            }

            retVal.resize(frameSize / sizeof(std::int16_t));
            std::memcpy(retVal.data(), decodeBuffer.data(), frameSize);

            return retVal;
        }
        return {};
    }
    return retVal;
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
    if (!m_recordingDevice)
    {
        if (m_deviceIndex > -1)
        {
            m_recordingDevice = alcCaptureOpenDevice(m_deviceList[m_deviceIndex].c_str(), SAMPLE_RATE, AL_FORMAT_MONO16, RECORD_BUFFER_SIZE);
        }
        else
        {
            m_recordingDevice = alcCaptureOpenDevice(nullptr, SAMPLE_RATE, AL_FORMAT_MONO16, RECORD_BUFFER_SIZE);
        }

        if (!m_recordingDevice)
        {
            LogE << "Failed opening device for recording" << std::endl;
        }
        else
        {
            m_active = true;
            alcCaptureStart(RECORDING_DEVICE);
        }
    }
    return m_recordingDevice != nullptr;
}