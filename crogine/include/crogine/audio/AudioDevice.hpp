/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <string>
#include <vector>

namespace cro
{
    /*!
    \brief Provides access to the current audio device, available audio devices
    and the ability to set a specific audio device as active.
    */
    class CRO_EXPORT_API AudioDevice final
    {
    public:
        /*!
        \brief Returns the name of the currently active device, if available
        */
        static const std::string& getActiveDevice();

        /*!
        \brief Returns a list of audio devices currently connected
        */
        static const std::vector<std::string>& getDeviceList();

        /*!
        \brief Attempts to set the given device as the active audio output
        else does nothing if the given device is not available.
        \param device The string name, retreived from getDeviceList(), to set active
        */
        static void setActiveDevice(const std::string&);
    };
}