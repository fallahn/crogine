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

#include <crogine/ecs/System.hpp>

#include <functional>

namespace cro
{
    /*!
    \brief Command struct.
    Each command encapsulates a mask of target IDs
    as well as a command in the form of a std::function
    */
    struct CRO_EXPORT_API Command final
    {
        std::uint64_t targetFlags = 0;
        std::function<void(Entity, float)> action;
    };

    /*
    \brief The command system is used to execute commands which are
    targeted at specific IDs
    */
    class CRO_EXPORT_API CommandSystem final : public cro::System
    {
    public:
        explicit CommandSystem(MessageBus&);

        /*!
        \brief Places a command on the command queue.
        Each frame the entire queue is processed and cleared,
        executing each command on each entity with a matching
        CommandTarget flag.
        \see CommandTarget
        */
        void sendCommand(const Command&);

        /*!
        \brief Process override
        */
        void process(float) override;

    private:
        std::vector<Command> m_commands;
        std::vector<Command> m_commandBuffer;
        std::size_t m_count;
    };
}