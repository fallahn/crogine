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

//#define RECORDING_DEVICE static_cast<ALCdevice*>(m_recordingDevice)
#define RECORDING_DEVICE m_recordingDevice
#define CAPTURE_CONTEXT(X) static_cast<cro::Detail::CaptureContext*>(X)

/*
Let's clarify a few things
When we talk about FRAME_SIZE we mean the number od SAMPLES in a frame.
FRAME_SIZE is directly proportional to SAMPLE_RATE where it is 1/25

A SAMPLE is made up of as many channels available so SAMPLE_SIZE = CHANNEL_COUNT
DATA_SIZE is the size in BYTES of a sample which is SAMPLE_SIZE * sizeof(T)
where T is float, int16 or uint8.

The sound recorder ALWAYS captures raw audio in STEREO (2 channels) at 48000Khz,
floating point. The capture callback feeds this through an SDL AudioStream which 
converts the sample rate and channel count to that which was requested when the
device was opened. It remains as float, and is written to a circular buffer
3 FRAMES in size. If the requested hardware do not support Stereo 48Khz float
opening the device will deliberately FAIL.

Data is stored in the circular buffer to give any optional effects on the
effects stack to be applied, before the data is fed into a second SDL
AudioStream which does the final conversion to the required output.

*/


using namespace cro;

namespace
{
    constexpr std::int32_t CAPTURE_CHANNELS = 2;
    constexpr std::int32_t CAPTURE_RATE = 48000;
    constexpr std::int32_t CAPTURE_FRAMES = 5; //number of frames which fit in the circular buffer

    void captureCallback(void* userdata, Uint8* stream, int len)
    {
        //this is automatically locked on its thread by SDL
        //HOWEVER we must lock the audio device when accessing
        //the capture context anywhere else.
        auto* ctx = CAPTURE_CONTEXT(userdata);

        SDL_AudioStreamPut(ctx->conversionStream, stream, len);

        auto available = SDL_AudioStreamAvailable(ctx->conversionStream);
        if (available > ctx->FrameSizeBytes)
        {
            SDL_AudioStreamGet(ctx->conversionStream, &ctx->circularBuffer[ctx->bufferInput], ctx->FrameSizeBytes);
            ctx->bufferInput = (ctx->bufferInput + ctx->FrameStride) % ctx->BufferSize;
            ctx->buffAvailable += ctx->FrameStride;

            if (ctx->bufferInput == ctx->bufferOutput)
            {
                ctx->bufferOutput = ((ctx->bufferInput - ctx->FrameStride) + ctx->BufferSize) % ctx->BufferSize;
                ctx->buffAvailable = ctx->FrameStride;
            }
        }
    }

    constexpr std::array OPUS_SAMPLE_RATES =
    {
        8000, 12000, 16000, 24000, 48000
    };

    constexpr std::uint32_t OPUS_MAX_PACKET_SIZE = 1276 * 3;
    constexpr std::uint32_t OPUS_BITRATE = 56000;

    const std::string MissingDevice("No Device Active");
    const std::string DefaultDevice("Default");

#ifdef CRO_DEBUG_
    std::int32_t pcmBufferCaptureSize = 0;

#endif
}

SoundRecorder::SoundRecorder()
    : m_deviceIndex     (-1),
    m_recordingDevice   (0),
    m_active            (false),
    m_channelCount      (0),
    m_sampleRate        (0),
    m_frameSize         (0),
    m_captureStream     (nullptr),
    m_captureContext    (0,0),
    m_outputStream      (nullptr)
{
    enumerateDevices();

#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            if (ImGui::Begin("Sound Recorder Debug"))
            {
                /*SDL_LockAudioDevice(m_recordingDevice);
                ImGui::PlotLines("Input Buffer", m_captureContext.circularBuffer, m_captureContext.BufferSize);
                SDL_UnlockAudioDevice(m_recordingDevice);*/

                //ImGui::Text("PCM Buffer Size %d", pcmBufferCaptureSize);
                //ImGui::PlotLines("Process Buffer", m_processBuffer.data(), m_processBuffer.size());
            }
            ImGui::End();
        });
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
    CRO_ASSERT(sampleRate >= 8000, "Sample rate must be at least 8KHz");
    m_channelCount = channelCount;
    m_sampleRate = sampleRate;
    m_frameSize = sampleRate / 25;
    
    if (!AudioRenderer::isValid())
    {
        LogE << "SoundRecorder::openDevice(): No valid audio renderer available." << std::endl;
        return false;
    }

    if (m_deviceList.empty())
    {
        LogE << "SoundRecorder::openDevice(): No recording devices are available" << std::endl;
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
        SDL_CloseAudioDevice(m_recordingDevice);
    }

    if (m_captureStream)
    {
        SDL_AudioStreamClear(m_captureStream);
        SDL_FreeAudioStream(m_captureStream);
    }

    if (m_outputStream)
    {
        SDL_AudioStreamClear(m_outputStream);
        SDL_FreeAudioStream(m_outputStream);
    }

    m_active = false;

    m_recordingDevice = 0;
    m_captureStream = nullptr;
    m_outputStream = nullptr;
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
        if (hasProcessedBuffer())
        {
            m_encoder->encode(m_processBuffer.data(), dst);
        }
    }
}

const std::int16_t* SoundRecorder::getPCMData(std::int32_t* count) const
{
    if (m_recordingDevice
        && hasProcessedBuffer())
    {
        SDL_AudioStreamPut(m_outputStream, m_processBuffer.data(), m_processBuffer.size() * sizeof(float));
        if (SDL_AudioStreamAvailable(m_outputStream) >= m_outputBuffer.size() * sizeof(std::int16_t))
        {
            SDL_AudioStreamGet(m_outputStream, m_outputBuffer.data(), m_outputBuffer.size() * sizeof(std::int16_t));
            *count = m_frameSize;
            return m_outputBuffer.data();
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
bool SoundRecorder::hasProcessedBuffer() const
{
    if (m_active)
    {
        SDL_LockAudioDevice(m_recordingDevice);
        if (m_captureContext.buffAvailable >= m_captureContext.FrameStride)
        {
            std::memcpy(m_processBuffer.data(), &m_circularBuffer[m_captureContext.bufferOutput], m_captureContext.FrameSizeBytes);
            m_captureContext.bufferOutput = (m_captureContext.bufferOutput + m_captureContext.FrameStride) % m_captureContext.BufferSize;
            m_captureContext.buffAvailable -= m_captureContext.FrameStride;

            SDL_UnlockAudioDevice(m_recordingDevice);


            //TODO run the copied buffer through the effects chain

            return true;
        }
        SDL_UnlockAudioDevice(m_recordingDevice);
    }
    return false;
}

void SoundRecorder::enumerateDevices()
{
    if (AudioRenderer::isValid())
    {
        const auto deviceCount = SDL_GetNumAudioDevices(SDL_TRUE);
        for (auto i = 0; i < deviceCount; ++i)
        {
            const auto* name = SDL_GetAudioDeviceName(i, SDL_TRUE);
            if (name)
            {
                m_deviceList.push_back(name);
            }
        }

        //return;
    }

    //m_deviceList.push_back("No Audio Device Available");
}

bool SoundRecorder::openSelectedDevice()
{
    if (!m_recordingDevice)
    {
        if (m_captureStream)
        {
            //this should never be true, but tidy up just in case
            SDL_FreeAudioStream(m_captureStream);
        }

        m_captureStream = SDL_NewAudioStream(AUDIO_F32, CAPTURE_CHANNELS, CAPTURE_RATE, AUDIO_F32, m_channelCount, m_sampleRate);

        if (!m_captureStream)
        {
            LogE << "Failed creating capture stream" << std::endl;
            return false;
        }



        if (m_outputStream)
        {
            SDL_FreeAudioStream(m_outputStream);
        }

        m_outputStream = SDL_NewAudioStream(AUDIO_F32, m_channelCount, m_sampleRate, AUDIO_S16, m_channelCount, m_sampleRate);

        if (!m_outputStream)
        {
            SDL_FreeAudioStream(m_captureStream);
            LogE << "Failed creating output stream" << std::endl;
            return false;
        }



        const auto frameBufferSize = m_frameSize * m_channelCount;
        const auto bufferSize = frameBufferSize * CAPTURE_FRAMES;
        m_circularBuffer.resize(bufferSize);

        m_captureContext = Detail::CaptureContext(bufferSize, frameBufferSize);
        m_captureContext.circularBuffer = m_circularBuffer.data();
        m_captureContext.conversionStream = m_captureStream;

        m_processBuffer.resize(frameBufferSize);
        m_outputBuffer.resize(frameBufferSize);

        SDL_AudioSpec recordingSpec = {};
        SDL_AudioSpec receivedSpec = {};

        SDL_zero(recordingSpec);
        recordingSpec.freq = CAPTURE_RATE;
        recordingSpec.format = AUDIO_F32;
        recordingSpec.channels = CAPTURE_CHANNELS;
        recordingSpec.samples = (CAPTURE_RATE / 25);// *CAPTURE_FRAMES;
        recordingSpec.callback = captureCallback;
        recordingSpec.userdata = &m_captureContext;

        if (m_deviceIndex > -1)
        {
            m_recordingDevice = SDL_OpenAudioDevice(m_deviceList[m_deviceIndex].c_str(), SDL_TRUE, &recordingSpec, &receivedSpec, SDL_FALSE);
        }
        else
        {
            m_recordingDevice = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &recordingSpec, &receivedSpec, SDL_FALSE);
        }

        if (!m_recordingDevice)
        {
            SDL_FreeAudioStream(m_captureStream);
            m_captureStream = nullptr;

            SDL_FreeAudioStream(m_outputStream);
            m_outputStream = nullptr;
        }
        else
        {
            SDL_PauseAudioDevice(m_recordingDevice, SDL_FALSE);
        }
        m_active = (m_recordingDevice != 0);

    }

    return m_recordingDevice != 0;
}