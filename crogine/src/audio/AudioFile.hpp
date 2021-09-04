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

#pragma once

#include "PCMData.hpp"

#include <crogine/core/Clock.hpp>

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
            \param chunkSize How much data is required to fill the PCMData
            \param looped Whether or not filling the struct should loop back
            to the beginning again once the end of the dile is reached
            \see PCMData
            */
            virtual const PCMData& getData(std::size_t chunkSize = 0, bool looped = false) const = 0;

            /*!
            \brief seek to a specific offset in the file
            \returns true if seek is successful, else false if out of bounds
            */
            virtual bool seek(cro::Time) = 0;
        };
    }
}