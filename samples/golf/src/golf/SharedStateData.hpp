/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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
#include "Networking.hpp"
#include "server/Server.hpp"

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
    std::uint32_t ballID = 0;
    std::uint32_t hairID = 0;
    std::uint32_t skinID = 0; //as loaded from the avatar data file
    bool flipped = false; //whether or not avatar flipped

    //these aren't included in serialise/deserialise
    std::vector<std::uint8_t> holeScores;
    std::uint8_t score = 0;
    std::uint8_t matchScore = 0;
    std::uint8_t skinScore = 0;
    glm::vec3 currentTarget = glm::vec3(0.f);

    bool isCPU = false;
};

struct ConnectionData final
{
    std::uint64_t peerID = 0;

    static constexpr std::uint8_t MaxPlayers = 4;
    std::uint8_t connectionID = MaxPlayers;

    std::uint8_t playerCount = 1;
    std::array<PlayerData, MaxPlayers> playerData = {};

    std::uint32_t pingTime = 0;

    std::vector<std::uint8_t> serialise() const;
    bool deserialise(const net::NetEvent::Packet&);
};

static constexpr float MinFOV = 60.f;
static constexpr float MaxFOV = 90.f;

struct SharedStateData final
{
    Server serverInstance;

    struct ClientConnection final
    {
        net::NetClient netClient;
        bool connected = false;
        bool ready = false;
        std::uint8_t connectionID = 4;

        std::vector<net::NetEvent> eventBuffer; //don't touch this while loading screen is active!!
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

    //available avatar models mapped to ID
    struct AvatarInfo final
    {
        std::uint32_t uid = 0;
        std::string modelPath;
        std::string audioscape;
    };
    std::vector<AvatarInfo> avatarInfo;

    //available hair models mapped to ID
    struct HairInfo final
    {
        std::uint32_t uid = 0;
        std::string modelPath;
        HairInfo(std::uint32_t i, const std::string& str)
            :uid(i), modelPath(str) {}
    };
    std::vector<HairInfo> hairInfo;

    struct TimeStats final
    {
        std::vector<std::int32_t> holeTimes; //millis
        std::int32_t totalTime = 0;
    };
    std::array<TimeStats, ConnectionData::MaxPlayers> timeStats = {};

    //our local player data
    std::uint64_t lobbyID = 0;
    std::uint64_t inviteID = 0;
    ConnectionData localConnectionData;
    cro::String targetIP = "255.255.255.255";
    std::array<std::int32_t, 4u> controllerIDs = {};

    //sent to server if hosting else rx'd from server
    //for brevity this only contains a directory name
    //within which a file named data.course is sought
    cro::String mapDirectory = "course_01";
    std::uint8_t scoreType = 0;
    std::uint8_t gimmeRadius = 0;
    std::uint8_t holeCount = 0; //0-1-2 all, front, back

    //printed by the error state
    std::string errorMessage;

    bool hosting = false;
    bool tutorial = false;
    std::size_t tutorialIndex = 0; //set in tutorial mode to decide which part to display
    std::size_t courseIndex = 0; //if hosting which course/billiard table we last chose.
    std::int32_t ballSkinIndex = 0; //billiards balls
    std::int32_t tableSkinIndex = 0; //billiards table

    //client settings
    bool usePostProcess = false;
    std::int32_t postProcessIndex = 0;
    std::string customShaderPath;
    InputBinding inputBinding;
    bool pixelScale = false;
    float fov = MinFOV;
    bool vertexSnap = false;
    float mouseSpeed = 1.f;
    bool invertX = false;
    bool invertY = false;
    bool showBeacon = true;
    float beaconColour = 1.f; //normalised rotation
    bool imperialMeasurements = false;
    float gridTransparency = 1.f;
    enum TreeQuality
    {
        Classic, Low, High
    };
    std::int32_t treeQuality = Low;
    bool hqShadows = false;
    bool logBenchmarks = false;
    bool showCustomCourses = true;

    std::int32_t baseState = 0; //used to tell which state we're returning to from errors etc
    std::unique_ptr<cro::ResourceCollection> sharedResources;
    std::vector<glm::uvec2> resolutions;
    std::vector<std::string> resolutionStrings;
};
