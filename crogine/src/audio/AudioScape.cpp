/*-----------------------------------------------------------------------

Matt Marchant 2021
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
#include <crogine/audio/AudioScape.hpp>

using namespace cro;

AudioScape::AudioScape()
    : m_audioResource(nullptr)
{

}

//public
bool AudioScape::loadFromFile(const std::string& path, AudioResource& audioResource)
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        m_audioResource = &audioResource;
        m_bufferIDs.clear();
        m_configs.clear();

        const auto& objs = cfg.getObjects();
        for (const auto& obj : objs)
        {
            if (obj.getName().empty())
            {
                LogW << "Parsed AudioScape with missing emitter name in " << path << std::endl;
                continue;
            }

            bool streaming = true; //fall back to this if missing so we don't accidentally try to load huge files
            std::string mediaPath;

            const auto& props = obj.getProperties();
            for (const auto& prop : props)
            {
                auto propName = prop.getName();
                if (propName == "path")
                {
                    mediaPath = prop.getValue<std::string>();
                }
                else if (propName == "streaming")
                {
                    streaming = prop.getValue<bool>();
                }
            }

            if (!mediaPath.empty() && cro::FileSystem::fileExists(mediaPath))
            {
                auto id = audioResource.load(mediaPath, streaming);
                if (id != -1)
                {
                    m_bufferIDs.insert(std::make_pair(mediaPath, id));
                    m_configs.insert(std::make_pair(obj.getName(), obj));
                }
            }

            if (m_configs.empty())
            {
                LogW << "No valid AudioScape definitions were loaded from " << path << std::endl;
                return false;
            }
            return true;
        }
    }

    return false;
}

AudioEmitter AudioScape::getEmitter(const std::string& name) const
{
    AudioEmitter emitter;
    if (m_audioResource
        && m_configs.count(name))
    {
        const auto& cfg = m_configs.at(name);
        const auto& props = cfg.getProperties();
        for (const auto& prop : props)
        {
            const auto& propName = prop.getName();
            if (name == "path")
            {
                emitter.setSource(m_audioResource->get(m_bufferIDs.at(name)));
            }
            else if (name == "looped")
            {
                emitter.setLooped(prop.getValue<bool>());
            }
            else if (name == "pitch")
            {
                emitter.setPitch(prop.getValue<float>());
            }
            else if (name == "volume")
            {
                emitter.setVolume(prop.getValue<float>());
            }
            else if (name == "mixer_channel")
            {
                std::uint8_t channel = static_cast<std::uint8_t>(std::min(15, std::max(0, prop.getValue<std::int32_t>())));
                emitter.setMixerChannel(channel);
            }
            else if (name == "attenuation")
            {
                emitter.setRolloff(prop.getValue<float>());
            }
            else if (name == "relative_listener")
            {
                //oh...
            }
        }        
    }
    else
    {
        LogW << name << " not found in AudioScape" << std::endl;
    }
    return emitter;
}