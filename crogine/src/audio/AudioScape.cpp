/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
    : m_audioResource   (nullptr),
    m_uid               (0)
{

}

//public
bool AudioScape::loadFromFile(const std::string& path, AudioResource& audioResource)
{
    m_configs.clear();
    m_name.clear();
    m_path.clear();
    m_audioResource = nullptr;
    m_uid = 0;

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        m_audioResource = &audioResource;
        m_path = path;

        if (const auto* p = cfg.findProperty("uid"); p != nullptr)
        {
            m_uid = p->getValue<std::uint32_t>();
        }

        const auto& objs = cfg.getObjects();
        for (const auto& obj : objs)
        {
            if (obj.getId().empty())
            {
                LogW << "Parsed AudioScape with missing emitter ID in " << path << std::endl;
                continue;
            }

            bool streaming = true; //fall back to this if missing so we don't accidentally try to load huge files
            std::string mediaPath;
            AudioConfig ac;

            const auto& props = obj.getProperties();
            for (const auto& prop : props)
            {
                auto propName = prop.getName();
                if (propName == "path")
                {
                    mediaPath = prop.getValue<std::string>();

                    if (!FileSystem::fileExists(FileSystem::getResourcePath() + mediaPath))
                    {
                        //try relative path
                        const auto relPath = FileSystem::getFilePath(path) + mediaPath;
                        //hmm usually this is because it's in a user dir and not the resource path
                        //but... sometimes it might be, so it won't work on macOS. (another reason to ditch it)
                        if (FileSystem::fileExists(/*FileSystem::getResourcePath() +*/ relPath)) 
                        {
                            mediaPath = relPath;
                        }
                        else
                        {
                            //prevent trying to load
                            mediaPath.clear();
                        }
                    }
                }
                else if (propName == "streaming")
                {
                    streaming = prop.getValue<bool>();
                }
                else if (propName == "looped")
                {
                    ac.looped = prop.getValue<bool>();
                }
                else if (propName == "pitch")
                {
                    ac.pitch = prop.getValue<float>();
                }
                else if (propName == "volume")
                {
                    ac.volume = prop.getValue<float>();
                }
                else if (propName == "mixer_channel")
                {
                    ac.channel = static_cast<std::uint8_t>(std::min(15, std::max(0, prop.getValue<std::int32_t>())));
                }
                else if (propName == "attenuation")
                {
                    ac.rolloff = prop.getValue<float>();
                }
                else if (propName == "relative_listener")
                {
                    //oh...
                }
            }

            if (!mediaPath.empty())
            {
                if (!streaming)
                {
                    ac.audioBuffer = audioResource.load(mediaPath, false);
                    if (ac.audioBuffer != -1)
                    {
                        m_configs.insert(std::make_pair(obj.getId(), ac));
                    }
                    else
                    {
                        LogW << "Failed opening file " << mediaPath << std::endl;
                    }
                }
                else
                {
                    //load the streaming source when creating the emitters, as streams need to be unique
                    ac.mediaPath = mediaPath;
                    m_configs.insert(std::make_pair(obj.getId(), ac));
                }
            }
            else
            {
                LogW << obj.getId() << " found no valid media file " << mediaPath << " in " << cro::FileSystem::getFileName(path) << std::endl;
            }
            
            m_name = cfg.getId();
        }

        if (m_configs.empty())
        {
            LogW << "No valid AudioScape definitions were loaded from " << path << std::endl;
            return false;
        }
        //m_name = cro::FileSystem::getFileName(path);
        return true;
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
        emitter.setMixerChannel(cfg.channel);
        emitter.setLooped(cfg.looped);
        emitter.setPitch(cfg.pitch);
        emitter.setRolloff(cfg.rolloff);
        emitter.setVolume(cfg.volume);

        if (cfg.audioBuffer > -1)
        {
            emitter.setSource(m_audioResource->get(cfg.audioBuffer));
        }
        else
        {
            //assume we're streaming
            auto id = m_audioResource->load(cfg.mediaPath, true);

            if (id > -1)
            {
                emitter.setSource(m_audioResource->get(id));
            }
        }
    }
    else
    {
        LogW << name << " not found in AudioScape" << std::endl;
    }
    return emitter;
}

bool AudioScape::hasEmitter(const std::string& name) const
{
    return m_configs.count(name) != 0;
}