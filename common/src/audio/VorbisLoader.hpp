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

#ifndef CRO_VORBIS_LOADER_HPP_
#define CRO_VORBIS_LOADER_HPP_

#define STB_VORBIS_HEADER_ONLY

#include "stb_vorbis.c"
#include "AudioFile.hpp"

#include <vector>

namespace cro
{
    namespace Detail
    {
        /*!
        \brief File loader for opening ogg vorbis files using stb_vorbis
        */
        class VorbisLoader final : public AudioFile
        {
        public:
            VorbisLoader();
            ~VorbisLoader();

            bool open(const std::string&) override;

            const PCMData& getData(std::size_t = 0) const override;

            bool seek(cro::Time) override;

        private:
            RaiiRWops m_file;
            stb_vorbis* m_vorbisFile;
            int32 m_channelCount;

            mutable PCMData m_dataChunk;
            mutable std::vector<int16> m_buffer;
        };
    }
}

#endif //CRO_VORBIS_LOADER_HPP_