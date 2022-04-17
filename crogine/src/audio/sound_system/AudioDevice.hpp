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

/*!
\brief Manages the existence of an OpenAL context for use with the
Sound System.
The Sound System is distinct from Audio classes in that it can be
used independently of the ECS and maintains its own context. The 
Sound System classes are heavily inspired by the audio module of
SFML Copyright (C) 2007-2021 Laurent Gomila (laurent@sfml-dev.org)
which is licensed under the zlib license (outlined above)
*/
namespace cro::Detail
{
    class AudioDevice final
    {
    public:
        AudioDevice();

        ~AudioDevice();

        AudioDevice(const AudioDevice&) = delete;
        AudioDevice(AudioDevice&&) noexcept = delete;

        AudioDevice& operator = (const AudioDevice&) = delete;
        AudioDevice& operator = (AudioDevice&&) noexcept = delete;


        /*!
        \brief Sets the global volume in a normalised range of 0 - 1.
        This is multiplied with the volume of any of the Sound System
        sources which are currently playing.
        \param vol Volume to set
        */
        static void setGlobalVolume(float vol);

        /*!
        \brief Returns the current global volume.
        \see setGlobalVolume()
        */
        static float getGlobalVolume();


    };
}