/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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
#include <crogine/detail/Assert.hpp>

#include <vector>

using namespace cro;

namespace
{
    std::int32_t autoID = std::numeric_limits<std::int32_t>::max();
}

AudioResource::AudioResource()
{
    m_fallback = std::make_unique<AudioBuffer>();
    std::vector<std::uint8_t> data(10, 0);
    dynamic_cast<AudioBuffer*>(m_fallback.get())->loadFromMemory(data.data(), 8, 22500, false, 10);
}

//public
bool AudioResource::load(std::int32_t ID, const std::string& path, bool streaming)
{
    if (!streaming &&
        m_sources.count(ID) > 0)
    {
        Logger::log("Data Source with ID " + std::to_string(ID) + " alread exists", Logger::Type::Error);
        return false;
    }

    std::unique_ptr<AudioSource> buffer;
    
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
        m_usedPaths.insert(std::make_pair(path, ID));
    }
    else
    {
        LogW << "Audio Resource: Failed loading " << path << std::endl;
    }
    return result;
}

std::int32_t AudioResource::load(const std::string& path, bool streaming)
{
    //streaming sources shouldn't be shared
    //cos, well, they're streaming

    if (!streaming &&
        m_usedPaths.count(path) != 0)
    {
        return m_usedPaths.at(path);
    }

    CRO_ASSERT(autoID > 0, "Something is very wrong if you've used this many IDs.");
    auto id = autoID;

    if (load(id, path, streaming))
    {
        autoID--;
        return id;
    }

    return -1;
}

const AudioSource& AudioResource::get(std::int32_t id) const
{
    if (m_sources.count(id) == 0) return *m_fallback;
    return *m_sources.find(id)->second;
}