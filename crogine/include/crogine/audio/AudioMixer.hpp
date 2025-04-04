/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <array>
#include <string>

namespace cro
{
    /*!
    \brief Multichannel Audio mixer class.
    The AudioMixer allows AudioEmitter components to group
    their outputs into one of 16 channels, each of which
    may have their volume adjusted, affecting all of the
    AudioEmitters currently routed through that channel.
    The AudioMixer also has a master volume channel which
    is applied to all subsequent channels. By default
    AudioEmitter components are assigned to channel 0
    \see AudioEmitter::setChannel()
    */
    class CRO_EXPORT_API AudioMixer final
    {
    public:
        /*!
        \brief Sets the master volume.
        Volume ranges from 0 - 1 for normal values
        but are clamped up to a value of 10 which,
        depending on the active subsystem, attempts
        to amplify the volume.
        */
        static void setMasterVolume(float);

        /*!
        \brief Returns the current master volume
        */
        static float getMasterVolume();

        /*!
        \brief Sets the volume of the given channel.
        Volume ranges from 0 - 1 for normal values
        but are clamped up to a value of 10 which,
        depending on the active subsystem, attempts
        to amplify the volume.
        \param vol The volume to which to set the channel
        \param channel ID (0 - 15) of the channel whose
        volume should be set.
        */
        static void setVolume(float vol, std::uint8_t channel);

        /*!
        \brief Returns the current volume of the requested channel
        */
        static float getVolume(std::uint8_t);

        /*!
        \brief Sets the pre-fade volume.
        This allows programmatically altering the volume at run-time
        without modifying the overall level of the mixer channel.
        Unlike main mixer channels these values are not stored in the
        application configuration between sessions, and are reset to
        default volume of 1 each time the application is started.
        \param vol The pre-fade volume to which to set the channel
        \param channel ID of the channel whose volume should be set.
        Valid values are 0 - 15.
        */
        static void setPrefadeVolume(float vol,std::uint8_t channel);

        /*!
        \brief Returns the pre-fade volume of the requested channel
        */
        static float getPrefadeVolume(std::uint8_t channel);

        /*!
        \brief Sets a label for a channel.
        For example you might want to set channel 0 to 'Effects'
        and channel 1 to 'Music' for easy reference. Channel names
        will appear in the console mixer window.
        \param label String to use as channel label
        \param channel To which to apply the label
        */
        static void setLabel(const std::string& label, std::uint8_t channel);

        /*!
        \brief Returns the current label assigned to the requested channel.
        \param channel Number of the channel from which to retrieve the label
        */
        static const std::string& getLabel(std::uint8_t channel);

        /*!
        \brief Returns true is a valid audio renderer was found to be available
        */
        static bool hasAudioRenderer();

        /*!
        \brief Prints the debug output of the active audio renderer, if it has
        been implemented. Call this between the begin/end of your own ImGui window
        */
        static void printDebug();


        static constexpr std::size_t MaxChannels = 16;

    private:
        static std::array<std::string, MaxChannels> m_labels;
        static std::array<float, MaxChannels> m_channels;
        static std::array<float, MaxChannels> m_prefadeChannels;
        static float m_masterVol;

        friend class AudioPlayerSystem;
        friend class AudioSystem;
    };
}