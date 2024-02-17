/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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
#include <vector>
#include <cstdint>

namespace cro
{
    class AudioEmitter;

    /*!
    \brief Abstract base class for audio data sources.
    AudioBuffers and AudioStreams are derived from this
    */
    class CRO_EXPORT_API AudioSource
    {
    public:
        AudioSource() : m_id(-1) {}
        virtual ~AudioSource() = default;

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
        std::int32_t getID() const { return m_id; }

        /*!
        \brief Attempts to load the file at the given path.
        */
        virtual bool loadFromFile(const std::string&) { return false; };

    protected:
        void setID(std::int32_t id) { m_id = id; }

        friend class AudioEmitter;

        //reference counts users so derived classes can tidy up if destroyed while still in use
        void addUser(AudioEmitter*) const;
        void removeUser(AudioEmitter*) const;
        void resetUsers();

    private:
        std::int32_t m_id;
        mutable std::vector<AudioEmitter*> m_users; //! < emitters currently using this source
    };
}