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
#include "AudioRenderer.hpp"

#include <crogine/audio/DynamicAudioStream.hpp>

using namespace cro;

DynamicAudioStream::DynamicAudioStream()
{
    //TODO setID(AudioRenderer::requestNewBufferableStream(&m_bufferedStream);
}

DynamicAudioStream::~DynamicAudioStream()
{
    if (getID() > -1)
    {
        resetUsers();

        AudioRenderer::deleteStream(getID());
        setID(-1);
    }
}

DynamicAudioStream::DynamicAudioStream(DynamicAudioStream&& other) noexcept
{
    auto id = getID();
    setID(other.getID());
    other.setID(id);
}

DynamicAudioStream& DynamicAudioStream::operator=(DynamicAudioStream&& other) noexcept
{
    if (&other != this)
    {
        auto id = getID();
        setID(other.getID());
        other.setID(id);
    }
    return *this;
}

//public
void DynamicAudioStream::updateBuffer(const std::vector<std::uint8_t>& data)
{
    CRO_ASSERT(m_bufferedStream, "");
    m_bufferedStream->updateBuffer(data);
}
 
//bool DynamicAudioStream::loadFromFile(const std::string& path)
//{
//    if (getID() > 0)
//    {
//        AudioRenderer::deleteStream(getID());
//        setID(-1);
//    }
//
//    setID(AudioRenderer::requestNewStream(path));
//    return getID() != -1;
//}