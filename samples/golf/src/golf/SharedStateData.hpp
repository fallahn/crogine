/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "InputBinding.hpp"
#include "server/Server.hpp"

#include <crogine/network/NetClient.hpp>
#include <crogine/core/String.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <string>
#include <array>
#include <memory>
#include <unordered_map>

struct PlayerData final
{
    cro::String name;
    std::array<std::uint8_t, 4u> avatarFlags = {1,0,3,6}; //indices into colour pairs
    std::uint32_t skinID = 0; //as loaded from the avatar data file
    bool flipped = false; //whether or not avatar flipped
    std::uint32_t ballID = 0;

    std::vector<std::uint8_t> holeScores;
    std::uint8_t score = 0;
    glm::vec3 currentTarget = glm::vec3(0.f); //TODO we don't really want to include this here as it's not needed for sync
};

struct ConnectionData final
{
    static constexpr std::uint8_t MaxPlayers = 4;
    std::uint8_t connectionID = MaxPlayers;

    std::uint8_t playerCount = 1;
    std::array<PlayerData, MaxPlayers> playerData = {};

    std::vector<std::uint8_t> serialise() const;
    bool deserialise(const cro::NetEvent::Packet&);
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

    //data of all players rx'd from server
    std::array<ConnectionData, 4u> connectionData = {};

    std::array<std::array<cro::Texture, 4u>, 4u> avatarTextures = {};
    std::array<cro::RenderTexture, 4u> nameTextures = {};

    //available ball models mapped to ID
    struct BallInfo final
    {
        cro::Colour tint;
        std::uint32_t uid = 0;
        std::string modelPath;
        BallInfo(cro::Colour c, std::uint32_t i, const std::string& str)
            : tint(c), uid(i), modelPath(str) {}
    };
    std::vector<BallInfo> ballModels;

    //available avatar sprites mapped to ID
    struct AvatarInfo final
    {
        std::uint32_t uid = 0;
        std::string modelPath;
        std::string audioscape;
    };
    std::vector<AvatarInfo> avatarInfo;

    //our local player data
    ConnectionData localConnectionData;
    cro::String targetIP = "255.255.255.255";
    std::array<std::int32_t, 4u> controllerIDs = {};

    //sent to server if hosting else rx'd from server
    //for brevity this only contains a directory name
    //within which a file named data.course is sought
    cro::String mapDirectory = "course_01";

    //printed by the error state
    std::string errorMessage;

    bool hosting = false;
    bool tutorial = false;
    std::size_t tutorialIndex = 0; //set in tutorial mode to decide which part to display
    std::size_t courseIndex = 0; //if hosting which course we last chose.

    //client settings
    bool usePostProcess = false;
    std::string customShaderPath;
    InputBinding inputBinding;
    bool pixelScale = true;

    std::int32_t baseState = 0; //used to tell which state we're returning to from errors etc
    std::unique_ptr<cro::ResourceCollection> sharedResources;
    std::vector<glm::uvec2> resolutions;
    std::vector<std::string> resolutionStrings;
};
