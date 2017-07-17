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

#ifndef CRO_AUDIO_RESOURCE_HPP_
#define CRO_AUDIO_RESOURCE_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/audio/AudioBuffer.hpp>

#include <string>
#include <unordered_map>

namespace cro
{
    /*!
    \brief Resource management for audio buffers.
    Audio buffers provide the audio data required by AudioSources.
    Buffers can be mapped to arbitrary integer values, such as that
    of an enum.
    */
    class CRO_EXPORT_API AudioResource final
    {
    public:
        AudioResource();

        ~AudioResource() = default;
        AudioResource(const AudioResource&) = delete;
        AudioResource(AudioResource&&) = default;

        AudioResource& operator = (const AudioResource&) = delete;
        AudioResource& operator = (AudioResource&&) = default;

        /*!
        \brief Loads an audio file and maps the resulting buffer to the given ID
        \param id Unique ID to map to the new buffer. If the ID is in use this will fail.
        \param path String containing the path to the file to load.
        \param streaming If set to true the requested file should be streamed from storage
        rather than loaded entirely into RAM
        */
        bool load(int32 id, const std::string& path, bool streaming = false);

        /*!
        \brief Attempts to return the loaded buffer mapped to the given ID
        If the requested ID is not found an empty buffer will be returned
        */
        const AudioBuffer& get(int32 id) const;

    private:

        AudioBuffer m_fallback;
        std::unordered_map<int32, AudioBuffer> m_buffers;
    };
}

#endif //CRO_AUDIO_RESOURCE_HPP_