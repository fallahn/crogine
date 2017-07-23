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

#ifndef CRO_AUDIO_FILE_HPP_
#define CRO_AUDIO_FILE_HPP_

#include "PCMData.hpp"

#include <string>

namespace cro
{
    namespace Detail
    {
        /*!
        \brief Abstract base class for audio file loader interface
        */
        class AudioFile
        {
        public:
            AudioFile() = default;
            virtual ~AudioFile() = default;

            /*!
            \brief Attempts to open a file for reading.
            Should return true if the file was successfully opened,
            else returns false.
            */
            virtual bool open(const std::string&) = 0;

            /*!
            \brief Returns a PCMData struct containing the requested
            chunk of decoded LPCM audio data.
            \see PCMData
            */
            virtual const PCMData& getData(std::size_t chunkSize = 0) const = 0;

            //TODO functions to seek within the file
        };
    }
}

#endif //CRO_AUDIO_FILE_HPP_