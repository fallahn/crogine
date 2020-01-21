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

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <string>

namespace cro
{
    /*!
    \brief Abstract base class for audio data sources.
    AudioBuffers and AudioStreams are derived from this
    */
    class CRO_EXPORT_API AudioDataSource
    {
    public:
        AudioDataSource() : m_id(-1) {}
        virtual ~AudioDataSource() = default;

        enum class Type
        {
            Stream, Buffer, None
        };
        /*!
        \brief Returns the type of the audio data
        */
        virtual Type getType() const = 0;

        /*
        \brief Returns the resource ID as assigned by the active AudioRenderer
        */
        int32 getID() const { return m_id; }

        /*!
        \brief Attempts to load the file at the given path.
        */
        virtual bool loadFromFile(const std::string&) = 0;

    protected:
        void setID(int32 id) { m_id = id; }

    private:
        int32 m_id;
    };
}