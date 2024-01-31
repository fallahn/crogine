/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "../M3UPlaylist.hpp"
#include "InputBinding.hpp"
#include "Networking.hpp"
#include "CommonConsts.hpp"
#include "PlayerData.hpp"
#include "server/Server.hpp"

#include <crogine/core/String.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <string>
#include <array>
#include <memory>
#include <unordered_map>

namespace cro
{
    class MultiRenderTexture;
}

struct ChatFonts final
{
    ImFont* buttonLarge = nullptr;
    float buttonHeight = 26.f;
};

struct ConnectionData final
{
    std::uint64_t peerID = 0;

    std::uint8_t connectionID = ConstVal::NullValue;

    std::uint8_t playerCount = 1;
    std::array<PlayerData, ConstVal::MaxPlayers> playerData = {};

    std::uint32_t pingTime = 0;
    std::uint8_t level = 0;

    std::vector<std::uint8_t> serialise() const;
    bool deserialise(const net::NetEvent::Packet&);
};

static constexpr float MinFOV = 60.f;
static constexpr float MaxFOV = 90.f;

struct SharedStateData final
{
    ChatFonts chatFonts;

    bool useOSKBuffer = false; //if true output of OSK is buffered here instead of sending codepoints
    cro::String OSKBuffer;

    struct MinimapData final
    {
        cro::MultiRenderTexture* mrt = nullptr;
        glm::vec3 teePos = glm::vec3(0.f);
        glm::vec3 pinPos = glm::vec3(0.f);
        cro::String courseName;
        std::int32_t holeNumber = -1;
        bool active = false;
    }minimapData;

    Server serverInstance;

    struct ClientConnection final
    {
        net::NetClient netClient;
        bool connected = false;
        bool ready = false;
        std::uint8_t connectionID = ConstVal::NullValue;

        std::uint64_t hostID = 0;
        std::vector<net::NetEvent> eventBuffer; //don't touch this while loading screen is active!!
    }clientConnection;

    //data of all players rx'd from server
    std::array<ConnectionData, ConstVal::MaxClients> connectionData = {};

    std::array<std::array<cro::Texture, ConstVal::MaxPlayers>, ConstVal::MaxClients> avatarTextures = {};
    std::array<cro::RenderTexture, ConstVal::MaxClients> nameTextures = {};

    //available ball models mapped to ID
    struct BallInfo final
    {
        cro::Colour tint;
        std::uint32_t uid = 0;
        std::string modelPath;
        bool rollAnimation = true;
        bool locked = false;
        std::uint64_t workshopID = 0;
        BallInfo() {}
        BallInfo(cro::Colour c, std::uint32_t i, const std::string& str)
            : tint(c), uid(i), modelPath(str) {}
    };
    std::vector<BallInfo> ballInfo;

    //available avatar models mapped to ID
    struct AvatarInfo final
    {
        std::uint32_t uid = 0;
        std::string modelPath;
        std::string texturePath;
        std::string audioscape;
        std::uint64_t workshopID = 0;
    };
    std::vector<AvatarInfo> avatarInfo;

    //available hair models mapped to ID
    struct HairInfo final
    {
        std::uint32_t uid = 0;
        std::string modelPath;
        std::uint64_t workshopID = 0;
        HairInfo() = default;
        HairInfo(std::uint32_t i, const std::string& str)
            :uid(i), modelPath(str) {}
    };
    std::vector<HairInfo> hairInfo;

    struct TimeStats final
    {
        std::vector<float> holeTimes; //seconds
        float totalTime = 0;
    };
    std::array<TimeStats, ConstVal::MaxPlayers> timeStats = {};

    //our local player data
    std::uint64_t lobbyID = 0;
    std::uint64_t inviteID = 0;
    ConnectionData localConnectionData;
    cro::String targetIP = "255.255.255.255";

    //sent to server if hosting else rx'd from server
    //for brevity this only contains a directory name
    //within which a file named data.course is sought
    cro::String mapDirectory = "course_01";
    std::uint8_t scoreType = 0;
    std::uint8_t gimmeRadius = 0;
    std::uint8_t holeCount = 0; //0-1-2 all, front, back
    std::uint8_t reverseCourse = 0; //play holes in reverse order
    std::uint8_t clubLimit = 0; //limit game to lowest player's clubs
    std::uint8_t nightTime = 0; //bool
    std::uint8_t weatherType = 0;

    //counts the number of holes actually played in elimination
    std::uint8_t holesPlayed = 0;

    //printed by the error state
    std::string errorMessage;

    bool hosting = false;
    bool tutorial = false;
    std::size_t tutorialIndex = 0; //set in tutorial mode to decide which part to display
    std::size_t courseIndex = 0; //if hosting which course/billiard table we last chose.
    std::int32_t ballSkinIndex = 0; //billiards balls
    std::int32_t tableSkinIndex = 0; //billiards table

    //IDs used by the unlock state to display new unlocks
    std::vector<std::int32_t> unlockedItems;

    //client settings
    bool usePostProcess = false;
    std::int32_t postProcessIndex = 0;
    std::string customShaderPath;
    InputBinding inputBinding;
    bool pixelScale = false;
    bool antialias = false;
    std::uint32_t multisamples = 0;
    float fov = MinFOV;
    bool vertexSnap = false;
    float mouseSpeed = 1.f;
    float swingputThreshold = 0.1f;
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
    bool showTutorialTip = true;
    bool showPuttingPower = false;
    bool showBallTrail = false;
    bool trailBeaconColour = true; //if false defaults to white
    bool fastCPU = true;
    std::int32_t enableRumble = 1;
    std::int32_t clubSet = 0;
    std::int32_t preferredClubSet = 0; //this is what the player chooses, may be overridden by game rules
    bool pressHold = false; //press and hold the action button to select power

    std::int32_t baseState = 0; //used to tell which state we're returning to from errors etc
    std::unique_ptr<cro::ResourceCollection> sharedResources;
    
    std::vector<glm::uvec2> resolutions;
    std::vector<std::string> resolutionStrings;

    std::unique_ptr<M3UPlaylist> m3uPlaylist;
};
