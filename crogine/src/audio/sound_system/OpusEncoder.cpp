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

#include <crogine/audio/sound_system/OpusEncoder.hpp>
#include <crogine/core/Log.hpp>

#include <opus.h>

using namespace cro;

#define OPUS_ENCODER static_cast<OpusEncoder*>(m_encoder)
#define OPUS_DECODER static_cast<OpusDecoder*>(m_decoder)

Opus::Opus(const Context& ctx)
    : m_encoder     (nullptr),
    m_decoder       (nullptr),
    m_frameSize     ((ctx.sampleRate / 25) /** ctx.channelCount*/),
    m_maxFrameSize  (m_frameSize * 3),
    m_outBuffer     (ctx.maxPacketSize),
    m_decodeBuffer  (m_maxFrameSize)
{
    std::int32_t err = 0;
    m_encoder = opus_encoder_create(ctx.sampleRate, ctx.channelCount, OPUS_APPLICATION_VOIP, &err);

    if (err < 0)
    {
        LogE << "Unable to create Opus encoder, err: " << opus_strerror(err) << std::endl;
        m_encoder = nullptr;
    }
    else
    {
        err = opus_encoder_ctl(OPUS_ENCODER, OPUS_SET_BITRATE(ctx.bitrate));
        if (err < 0)
        {
            LogE << "Failed to set encoder bitrate to " << ctx.bitrate << ": " << opus_strerror(err) << std::endl;
        }
    }

    m_decoder = opus_decoder_create(ctx.sampleRate, ctx.channelCount, &err);
    if (err < 0)
    {
        LogE << "Unable to create Opus decoder, err: " << opus_strerror(err) << std::endl;
        m_decoder = nullptr;
    }
}

Opus::~Opus()
{
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
void Opus::encode(const std::int16_t* data, std::int32_t dataSize, std::vector<std::uint8_t>& dst) const
{
    auto byteCount = opus_encode(OPUS_ENCODER, data, dataSize, m_outBuffer.data(), m_outBuffer.size());
    if (byteCount > 0)
    {
        dst.resize(byteCount);
        std::memcpy(dst.data(), m_outBuffer.data(), byteCount);
    }
    else
    {
        dst.clear();
        LogI << opus_strerror(byteCount) << std::endl;
    }
}

std::vector<std::int16_t> Opus::decode(const std::vector<std::uint8_t>& packet) const
{
    std::vector<std::int16_t> retVal;

    if (m_decoder)
    {
        auto sampleCount = opus_decode(OPUS_DECODER, packet.data(), packet.size(), m_decodeBuffer.data(), m_decodeBuffer.size(), 0);
        if (sampleCount > 0)
        {
            retVal.resize(sampleCount);
            std::memcpy(retVal.data(), m_decodeBuffer.data(), sampleCount * sizeof(std::int16_t));

            return retVal;
        }
    }
    return retVal;
}