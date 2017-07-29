/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/audio/AudioResource.hpp>
#include <crogine/audio/AudioBuffer.hpp>
#include <crogine/audio/AudioStream.hpp>
#include <crogine/core/Log.hpp>

using namespace cro;

AudioResource::AudioResource()
{
    m_fallback = std::make_unique<AudioBuffer>();
    std::vector<uint8> data(10);
    dynamic_cast<AudioBuffer*>(m_fallback.get())->loadFromMemory(data.data(), 8, 22500, false, 10);
}

//public
bool AudioResource::load(int32 ID, const std::string& path, bool streaming)
{
    if (m_sources.count(ID) > 0)
    {
        Logger::log("Data Source with ID " + std::to_string(ID) + " alread exists", Logger::Type::Error);
        return false;
    }

    std::unique_ptr<AudioDataSource> buffer;
    
    if (streaming)
    {
        buffer = std::make_unique<AudioStream>();
    }
    else
    {
        buffer = std::make_unique<AudioBuffer>();
    }
    
    auto result = buffer->loadFromFile(path);
    if (result)
    {
        m_sources.insert(std::make_pair(ID, std::move(buffer)));
    }
    return result;
}

const AudioDataSource& AudioResource::get(int32 id) const
{
    if (m_sources.count(id) == 0) return *m_fallback;
    return *m_sources.find(id)->second;
}