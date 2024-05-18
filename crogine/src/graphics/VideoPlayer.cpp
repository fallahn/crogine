/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/graphics/VideoPlayer.hpp>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "../detail/GLCheck.hpp"

#include <crogine/core/FileSystem.hpp>
#include <crogine/gui/Gui.hpp>

#include <string>

namespace
{
    const std::string ShaderVertex =
R"(
ATTRIBUTE vec2 a_position;
ATTRIBUTE vec2 a_texCoord0;
ATTRIBUTE vec4 a_colour;

uniform mat4 u_worldMatrix;
uniform mat4 u_projectionMatrix;

VARYING_OUT vec2 v_texCoord;
VARYING_OUT vec4 v_colour;

void main()
{
    gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
    v_texCoord = vec2(a_texCoord0.x, 1.0 - a_texCoord0.y);
    v_colour = a_colour;
})";


    //shader based on example at https://github.com/phoboslab/pl_mpeg
    const std::string ShaderFragment =
        R"(
uniform sampler2D u_texture; //Y component but renamed to match SimpleQuad expectations
uniform sampler2D u_textureCB;
uniform sampler2D u_textureCR;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

    const mat4 rec601 = 
        mat4(
            1.16438,  0.00000,  1.59603, -0.87079,
            1.16438, -0.39176, -0.81297,  0.52959,
            1.16438,  2.01723,  0.00000, -1.08139,
            0.0, 0.0, 0.0, 1.0
            );

void main()
{
    float y = TEXTURE(u_texture, v_texCoord).r;
    float cb = TEXTURE(u_textureCB, v_texCoord).r;
    float cr = TEXTURE(u_textureCR, v_texCoord).r;

    FRAG_OUT = vec4(y, cb, cr, 1.0) * rec601 * v_colour;
})";

    //this is always 2, according to PLM.
    static constexpr std::uint32_t ChannelCount = 2;
    static constexpr std::uint32_t AudioBufferSize = PLM_AUDIO_SAMPLES_PER_FRAME * ChannelCount;
}

using namespace cro;

//C senor.
void cro::videoCallback(plm_t* mpg, plm_frame_t* frame, void* user)
{
    auto* videoPlayer = static_cast<VideoPlayer*>(user);
    videoPlayer->updateTexture(videoPlayer->m_y.getGLHandle(), &frame->y);
    videoPlayer->updateTexture(videoPlayer->m_cb.getGLHandle(), &frame->cb);
    videoPlayer->updateTexture(videoPlayer->m_cr.getGLHandle(), &frame->cr);
}

void cro::audioCallback(plm_t*, plm_samples_t* samples, void* user)
{
    auto* videoPlayer = static_cast<VideoPlayer*>(user);
    videoPlayer->m_audioStream.pushData(samples->interleaved);   
}

VideoPlayer::VideoPlayer()
    : m_plm             (nullptr),
    m_looped            (false),
    m_timeAccumulator   (0.f),
    m_frameTime         (0.f),
    m_state             (State::Stopped)
{
    //TODO we don't really want to create a shader for EVERY instance
    //but on the other hand why would I play a lot of videos at once?

    if (!m_shader.loadFromString(ShaderVertex, ShaderFragment))
    {
        LogW << "Failed creating shader for video renderer" << std::endl;
    }
    else
    {
        //only need to set these once as we fix the layout (see update buffer)
        glUseProgram(m_shader.getGLHandle());
        glUniform1i(m_shader.getUniformID("u_texture"), 0);
        glUniform1i(m_shader.getUniformID("u_textureCR"), 1);
        glUniform1i(m_shader.getUniformID("u_textureCB"), 2);
    }
}

//VideoPlayer::VideoPlayer(VideoPlayer&& other)
//    : m_plm             (other.m_plm),
//    m_looped            (other.m_looped),
//    m_timeAccumulator   (0.f),
//    m_frameTime         (other.m_frameTime),
//    m_state             (other.m_state)
//{
// ehhhhhhh.... might not be worth doing
//}

VideoPlayer::~VideoPlayer()
{
    if (m_plm)
    {
        stop();

        plm_destroy(m_plm);
    }
}

bool VideoPlayer::loadFromFile(const std::string& path)
{
    auto fullPath = cro::FileSystem::getResourcePath() + path;

    //remove existing file first
    if (m_state == State::Playing)
    {
        stop();
    }   
    
    if (m_plm)
    {
        plm_destroy(m_plm);
        m_plm = nullptr;
    }   
    
    
    if (m_shader.getGLHandle() == 0)
    {
        LogE << "Unable to open file " << cro::FileSystem::getFileName(path) << ": shader not loaded";
        return false;
    }

    if (!cro::FileSystem::fileExists(fullPath))
    {
        LogE << "Unable to open file " << path << ": file not found" << std::endl;
        return false;
    }


    //load the file
    m_plm = plm_create_with_filename(fullPath.c_str());

    if (!m_plm)
    {
        LogE << "Failed creating video player instance (incompatible file?)" << cro::FileSystem::getFileName(path) << std::endl;
        return false;
    }


    auto width = plm_get_width(m_plm);
    auto height = plm_get_height(m_plm);
    auto frameRate = plm_get_framerate(m_plm);

    if (width == 0 || height == 0 || frameRate == 0)
    {
        LogE << cro::FileSystem::getFileName(path) << ": invalid file properties" << std::endl;
        plm_destroy(m_plm);
        m_plm = nullptr;

        return false;
    }

    
    m_frameTime = 1.f / frameRate;

    //the plane sizes aren't actually the same
    //but this sets the texture property used
    //by the output sprite so that it matches
    //the render buffer size (which is this value)
    m_y.create(width, height, cro::ImageFormat::A);
    m_cr.create(width, height, cro::ImageFormat::A);
    m_cb.create(width, height, cro::ImageFormat::A);
    m_outputBuffer.create(width, height, false);

    m_quad.setTexture(m_y);
    m_quad.setShader(m_shader);

    plm_set_video_decode_callback(m_plm, videoCallback, this);

    //enable audio
    plm_set_audio_decode_callback(m_plm, audioCallback, this);

    if (plm_get_num_audio_streams(m_plm) > 0)
    {
        auto sampleRate = plm_get_samplerate(m_plm);
        m_audioStream.init(ChannelCount, sampleRate);
        m_audioStream.hasAudio = true;

        plm_set_audio_lead_time(m_plm, static_cast<double>(AudioBufferSize) / sampleRate);
    }
    else
    {
        m_audioStream.hasAudio = false;
    }

    plm_set_loop(m_plm, m_looped ? 1 : 0);

    return true;
}

void VideoPlayer::update(float dt)
{
    m_timeAccumulator += dt;

    static constexpr float MaxTime = 1.f;
    if (m_timeAccumulator > MaxTime)
    {
        m_timeAccumulator = 0.f;
    }

    if (m_plm)
    {
        CRO_ASSERT(m_frameTime > 0, "");
        while (m_timeAccumulator > m_frameTime)
        {
            m_timeAccumulator -= m_frameTime;

            if (m_state == State::Playing)
            {
                plm_decode(m_plm, m_frameTime);

                updateBuffer();

                if (plm_has_ended(m_plm))
                {
                    stop();
                }
            }
        }
    }
}

void VideoPlayer::play()
{
    if (m_plm == nullptr)
    {
        LogE << "No video file loaded " << std::endl;
        return;
    }

    if (m_frameTime == 0)
    {
        return;
    }

    if (m_state == State::Playing)
    {
        return;
    }

    m_timeAccumulator = 0.f;
    m_state = State::Playing;
    
    if (m_audioStream.hasAudio)
    {
        m_audioStream.play();
    }
}

void VideoPlayer::pause()
{
    if (m_state == State::Playing)
    {
        m_state = State::Paused;
        m_audioStream.pause();
    }
}

void VideoPlayer::stop()
{
    if (m_state != State::Stopped)
    {
        m_state = State::Stopped;
        m_audioStream.stop();

        if (m_plm)
        {
            //rewind the file
            plm_seek(m_plm, 0, FALSE);

            //clear the buffer else we repeat the last frame
            m_outputBuffer.clear();
            m_outputBuffer.display();
        }
    }
}

void VideoPlayer::seek(float position)
{
    if (m_plm)
    {
        plm_seek(m_plm, position, FALSE);

        if (m_state != State::Playing)
        {
            updateBuffer();
        }
    }
}

float VideoPlayer::getDuration() const
{
    if (m_plm)
    {
        return static_cast<float>(plm_get_duration(m_plm));
    }
    return 0.f;
}

float VideoPlayer::getPosition() const
{
    if (m_plm)
    {
        return static_cast<float>(plm_get_time(m_plm));
    }
    return 0.f;
}

void VideoPlayer::setLooped(bool looped)
{
    m_looped = looped;

    if (m_plm)
    {
        plm_set_loop(m_plm, looped ? 1 : 0);
    }
}

//private
void VideoPlayer::updateTexture(std::uint32_t textureID, plm_plane_t* plane)
{
    CRO_ASSERT(textureID != 0, "");
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, plane->width, plane->height, 0, GL_RED, GL_UNSIGNED_BYTE, plane->data);
}

void VideoPlayer::updateBuffer()
{
    //remaining shader setup is handled by SimpleQuad
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_cr.getGLHandle());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_cb.getGLHandle());

    m_outputBuffer.clear();
    m_quad.draw();
    m_outputBuffer.display();
}

bool VideoPlayer::AudioStream::onGetData(cro::SoundStream::Chunk& chunk)
{
    const auto getChunkSize = [&]()
    {
        auto in = static_cast<std::int32_t>(m_bufferIn);
        auto out = static_cast<std::int32_t>(m_bufferOut);
        auto chunkSize = (in - out) + static_cast<std::int32_t>(m_inBuffer.size());
        chunkSize %= m_inBuffer.size();

        chunkSize = std::min(chunkSize, static_cast<std::int32_t>(m_outBuffer.size()));

        return chunkSize;
    };


    auto chunkSize = getChunkSize();

    //wait for the buffer to fill
    while (chunkSize == 0
        && getStatus() == AudioStream::Status::Playing)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        chunkSize = getChunkSize();
    }

    for (auto i = 0; i < chunkSize; ++i)
    {
        auto idx = (m_bufferOut + i) % m_inBuffer.size();
        m_outBuffer[i] = m_inBuffer[idx];
    }

    chunk.sampleCount = chunkSize;
    chunk.samples = m_outBuffer.data();

    m_bufferOut = (m_bufferOut + chunkSize) % m_inBuffer.size();

    return true;
}

void VideoPlayer::AudioStream::init(std::uint32_t channels, std::uint32_t sampleRate)
{
    stop();
    initialise(channels, sampleRate, 0);
}

void VideoPlayer::AudioStream::pushData(float* data)
{
    for (auto i = 0u; i < AudioBufferSize; ++i)
    {
        auto index = (m_bufferIn + i) % m_inBuffer.size();

        std::int16_t sample = data[i] * std::numeric_limits<std::int16_t>::max();
        m_inBuffer[index] = sample;
    }

    m_bufferIn = (m_bufferIn + AudioBufferSize) % m_inBuffer.size();
}