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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <string>
#include <unordered_map>

namespace cro
{
    class AudioResource;

    /*!
    \brief Parses AudioScape configuration files.
    To externally define the properties of an AudioEmitter AudioScape
    files can be created using the default cro::ConfigFile format
    and parsed with this class. An AudioScape configuration file has
    the following format:
    \begincode
    audioscape <name>
    {
        emitter <emitter_name>
        {
            path = "path/to/media.file"
            looped = <bool>
            streaming = <bool>
            volume = <float>
            mixer_channel = <0 - 15>
            attenuation = <float> //analogous to AudioEmitter::rolloff
            pitch = <float> 0.5 - 2.0
            relative_listener = <bool>
        }

        emitter <other_name>
        {
            path = "path/to/another_media.file"
            looped = <bool>
            streaming = <bool>
            volume = <float>
            mixer_channel = <0 - 15>
            attenuation = <float>
        }
    }
    \endcode
    The AudioScape class works similarly to a SpriteSheet - it requires
    a reference to a valid AudioResource to manage the source files and,
    once loaded, can be used to return AudioEmitter components configured
    with the named definitions in the configuration file.
    */
    class CRO_EXPORT_API AudioScape final
    {
    public:
        AudioScape();

        /*!
        \brief Attempts to load a configuration file at the given path
        \param path A path to the configuration file containing the AudioScape definition
        \param audioResource A reference to the AudioResource used to managed the loaded data
        \returns true if the file was parsed successfully, else false.
        */
        bool loadFromFile(const std::string& path, AudioResource& audioResource);

        /*!
        \brief Returns the named AudioEmitter if it is found, else returns an emitter
        with default settings
        \param name Name of the emitter as it appears in the configuration file
        */
        AudioEmitter getEmitter(const std::string& name) const;

        /*!
        \brief Returns true if the AudioScape contains an emitter definition with the given name
        \param name Name of the emitter to query
        */
        bool hasEmitter(const std::string& name) const;

        /*!
        \brief Returns the name from which this AudioScape was loaded, or an empty string
        */
        const std::string& getName() const { return m_name; }

        /*!
        \brief Returns the UID loaded from the AudioScape file, or 0 if there was none
        */
        std::uint32_t getUID() const { return m_uid; }

    private:
        AudioResource* m_audioResource;
        std::string m_name;
        std::uint32_t m_uid;

        struct AudioConfig final
        {
            float volume = 0.5f;
            float pitch = 1.f;
            float rolloff = 0.f;
            std::int32_t audioBuffer = -1;
            std::string mediaPath;

            bool looped = false;
            std::uint8_t channel = 0;
        };
        std::unordered_map<std::string, AudioConfig> m_configs;
    };
}