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
#include "../al.h"

//silence deprecated openal warnings
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <opus.h>

#define RECORDING_DEVICE static_cast<ALCdevice*>(m_recordingDevice)
#define OPUS_ENCODER static_cast<OpusEncoder*>(m_encoder)

using namespace cro;

namespace
{
    constexpr ALCsizei RECORD_BUFFER_SIZE = 2048;
    constexpr std::uint32_t PCMBufferSize = 16384; //testing shows we cap either 220 or 441 frames at a time (@60hz)
    constexpr std::uint32_t PCMBufferWrapSize = PCMBufferSize - (PCMBufferSize / 10); //so we want to wrap our pointer with enough room to spare
    constexpr std::uint32_t SAMPLE_RATE = 22050;
    constexpr std::uint32_t SAMPLE_SIZE = sizeof(std::uint16_t);

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
    m_pcmBuffer         (PCMBufferSize),
    m_pcmBufferOffset   (0)
{
    enumerateDevices();

#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            if (ImGui::Begin("Sound Recorder Debug"))
            {
                ImGui::Text("PCM Buffer Size %d", pcmBufferCaptureSize);
            }
            ImGui::End();
        });
#endif
}

SoundRecorder::~SoundRecorder()
{
    stop();
    closeDevice();
}

//public
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
        alcCaptureCloseDevice(RECORDING_DEVICE);
    }
    m_recordingDevice = nullptr;
}

bool SoundRecorder::start()
{
    if (m_active)
    {
        return true;
    }

    if (!AudioRenderer::isValid())
    {
        LogE << "SoundRecorder::start(): No valid audio renderer available." << std::endl;
        return false;
    }

    //opens device if not yet open
    if (!openSelectedDevice())
    {
        return false;
    }


    alcCaptureStart(RECORDING_DEVICE);
    m_active = true;
    m_pcmBufferOffset = 0;

    return true;
}

void SoundRecorder::stop()
{
    if (m_recordingDevice)
    {
        alcCaptureStop(RECORDING_DEVICE);
    }
    m_active = false;
}

bool SoundRecorder::isActive() const
{
    return m_active;
}

const std::uint8_t* SoundRecorder::getEncodedPackets(std::int32_t* count) const
{
    if (m_recordingDevice && m_active)
    {
        ALCint sampleCount = 0;

        alcGetIntegerv(RECORDING_DEVICE, ALC_CAPTURE_SAMPLES, 1, &sampleCount);
        alcCaptureSamples(RECORDING_DEVICE, m_pcmBuffer.data() + m_pcmBufferOffset, sampleCount);

        m_pcmBufferOffset = (m_pcmBufferOffset + (sampleCount * SAMPLE_SIZE)) % PCMBufferWrapSize;


        //TODO once the pcm buffer reaches an encodable size
        //encode the buffer and reset the buffer offset

#ifdef CRO_DEBUG_
        pcmBufferCaptureSize = sampleCount;
#endif

    }



    *count = 0;

    return nullptr;
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
    }
    return m_recordingDevice != nullptr;
}