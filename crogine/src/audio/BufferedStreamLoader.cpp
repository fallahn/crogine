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

#include "BufferedStreamLoader.hpp"

#include <cstring>

using namespace cro;
using namespace cro::Detail;

namespace
{
    std::vector<std::int16_t> silentBuffer(2048);
}

BufferedStreamLoader::BufferedStreamLoader()
    : m_channelCount(1),
    m_sampleCount   (0)
{
    m_dataChunk.format = PCMData::Format::MONO16;
    m_dataChunk.frequency = 22050;

    std::fill(silentBuffer.begin(), silentBuffer.end(), 0);
}

BufferedStreamLoader::~BufferedStreamLoader()
{

}

//public
bool BufferedStreamLoader::open(const std::string&)
{
    return true;
}

const PCMData& BufferedStreamLoader::getData(std::size_t, bool) const
{
    if (m_doubleBuffer.empty())
    {
        m_dataChunk.data = silentBuffer.data();
        m_dataChunk.size = silentBuffer.size() * sizeof(std::uint16_t);

        return m_dataChunk;
    }

    std::scoped_lock l(m_mutex);
    m_buffer.swap(m_doubleBuffer);
    m_doubleBuffer.clear();

    m_dataChunk.data = m_buffer.data();
    m_dataChunk.size = m_buffer.size() * sizeof(std::uint16_t);
    
    return m_dataChunk;
}

void BufferedStreamLoader::updateBuffer(const std::vector<std::uint8_t>& b)
{
    std::scoped_lock l(m_mutex);
    auto oldSize = m_doubleBuffer.size();
    m_doubleBuffer.resize((b.size() / sizeof(std::int16_t)) + oldSize);
    std::memcpy(&m_doubleBuffer[oldSize], b.data(), b.size());
}