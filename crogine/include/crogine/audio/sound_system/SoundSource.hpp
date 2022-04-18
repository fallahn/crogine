/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <cstdint>

namespace cro
{
    /*!
    \brief Base class for sound sources in the Sound System
    The Sound System is distinct from Audio classes in that it can be
    used independently of the ECS. The Sound System classes are
    heavily inspired by the audio module of SFML 
    Copyright (C) 2007-2021 Laurent Gomila (laurent@sfml-dev.org)
    which is licensed under the zlib license (outlined above)

    NOTE that this *requires* OpenAL, so if the default AudioRenderer
    is not being used then Sound System classes will not
    work, unless a valid OpenAL context is manually created.

    This class groups common settings such as volume control, shared
    by all sound sources in the Sound System.
    */
    class CRO_EXPORT_API SoundSource
    {
    public:

        enum class Status
        {
            Stopped, Paused, Playing
        };

        /*!
        \brief Copy constructor
        */
        SoundSource(const SoundSource&);

        /*!
        \brief Assignment operator
        */
        SoundSource& operator = (const SoundSource&);

        //TODO move operators?

        virtual ~SoundSource();

        /*!
        \brief Sets the normalised volume in the range 0 - 1
        \param vol Volume of the sound source where 0 is silent
        and 1 is full volume. The volume is multiplied with the
        global sound volume, set with the Listener properties
        of the AudioRenderer
        \see AudioRenderer::setListenerVolume();
        */
        void setVolume(float vol);

        /*!
        \brief Returns the current volume of the SoundSource
        \see setVolume()
        */
        float getVolume() const;

        /*!
        \brief Start or resume playback of the SoundSource
        */
        virtual void play() = 0;

        /*!
        \brief Pause playback of the SoundSource
        */
        virtual void pause() = 0;

        /*!
        \brief Stop playback of the SoundSource
        This also rewinds the SoundSource position to
        the beginning, unlike pause()
        */
        virtual void stop() = 0;

        /*!
        \brief Returns the current status of the SoundSource
        */
        virtual Status getStatus() const;


    protected:

        SoundSource();

        std::uint32_t m_alSource;
    };
}
