/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "Server.hpp"
#include "InputBinding.hpp"

#include <crogine/network/NetClient.hpp>
#include <crogine/core/String.hpp>

#include <string>

struct PlayerData final
{
    cro::String name;
    //TODO other stuff like skin data
};

struct SharedStateData final
{
    Server serverInstance;

    struct ClientConnection final
    {
        cro::NetClient netClient;
        bool connected = false;
        bool ready = false;
        std::uint8_t connectionID = 4;
    }clientConnection;

    //keybindings for all local players
    std::array<InputBinding, 4u> inputBindings = {};

    //data of all players rx'd from server
    std::array<PlayerData, 4u> playerData = {};

    //our local player data
    PlayerData localPlayer;
    cro::String targetIP;
    std::uint8_t localPlayerCount = 1;

    //so we know what to try loading
    cro::String mapName;

    //set before pushing error state to display
    std::string errorMessage;

    enum class HostState
    {
        None, Local, Network
    }hostState = HostState::None;
};
