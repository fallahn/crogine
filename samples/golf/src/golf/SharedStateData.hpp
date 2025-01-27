/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include "CommonConsts.hpp"
#include "PlayerData.hpp"
#include "LeagueNames.hpp"
#include "Tournament.hpp"
#include "server/Server.hpp"

#include <crogine/audio/sound_system/Playlist.hpp>
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

struct MenuSky final
{
    glm::vec3 sunPos = glm::vec3(-0.505335f, 0.62932f, 0.590418f);
    cro::Colour sunColour = cro::Colour::White;

    cro::Colour skyTop = cro::Colour(0.723f, 0.847f, 0.792f, 1.f);
    cro::Colour skyBottom = 0xfff8e1ff;
    float stars = 0.f;

    MenuSky() = default;

    constexpr MenuSky(glm::vec3 sPos, cro::Colour sCol, cro::Colour top, cro::Colour bottom, float s)
        : sunPos(sPos), sunColour(sCol), skyTop(top), skyBottom(bottom), stars(s) {}
};

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

enum class GameMode
{
    Career, FreePlay, Tutorial, Clubhouse, Tournament
};

struct SharedCourseData;
struct SharedStateData final
{
    //used to set background colour in main menu and clubhouse
    MenuSky menuSky;

    cro::Playlist playlist;

    bool showCredits = false;
    std::int32_t leagueTable = 0; //which league table to display when opening league browser
    SharedCourseData* courseData = nullptr; //only valid when MenuState is active

    ChatFonts chatFonts;

    bool useOSKBuffer = false; //if true output of OSK is buffered here instead of sending codepoints
    cro::String OSKBuffer;

    struct ActiveInput final
    {
        enum
        {
            Keyboard, XBox, PS
        };
    };
    std::int32_t activeInput = ActiveInput::Keyboard; //NOTE only updated by driving range and main game

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
    };
    ClientConnection clientConnection;
    ClientConnection voiceConnection;

    //data of all players rx'd from server
    std::array<ConnectionData, ConstVal::MaxClients> connectionData = {};

    std::array<std::array<cro::Texture, ConstVal::MaxPlayers>, ConstVal::MaxClients> avatarTextures = {};
    std::array<cro::RenderTexture, ConstVal::MaxClients> nameTextures = {};

    //available ball models mapped to ID
    struct BallInfo final
    {
        enum
        {
            Regular, Unlock, Custom
        }type = Regular;
        cro::String label;
        std::string modelPath;

        std::uint64_t workshopID = 0;
        cro::Colour tint;
        std::uint32_t uid = 0;
        float previewRotation = 0.f;
        bool rollAnimation = true;
        bool locked = false;
        BallInfo() {}
        BallInfo(cro::Colour c, std::uint32_t i, const std::string& str)
            : modelPath(str), tint(c), uid(i) {}
    };
    std::vector<BallInfo> ballInfo;

    //available avatar models mapped to ID
    struct AvatarInfo final
    {
        enum
        {
            Regular, Unlock, Custom
        }type = Regular;

        std::uint32_t uid = 0;
        std::string modelPath;
        std::string texturePath;
        std::string audioscape;
        std::uint64_t workshopID = 0;
        bool locked = false;
    };
    std::vector<AvatarInfo> avatarInfo;

    //available hair models mapped to ID
    struct HairInfo final
    {
        enum
        {
            Regular, Unlock, Custom
        }type = Regular;
        cro::String label;

        std::uint32_t uid = 0;
        std::string modelPath;
        std::uint64_t workshopID = 0;
        bool locked = false;
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
    std::uint8_t randomWind = 0; //bool
    std::uint8_t windStrength = 0; //1-5 but stored 0-4 for ease of iteration
    std::int32_t leagueRoundID = 0; //which league we're playing in
    std::int32_t quickplayOpponents = 0; //1-3 if quickplay, 0 to disable

    //counts the number of holes actually played in elimination
    std::uint8_t holesPlayed = 0;

    //printed by the error state
    std::string errorMessage;

    bool hosting = false;
    bool hasMulligan = false;
    GameMode gameMode = GameMode::FreePlay;
    std::size_t tutorialIndex = 0; //set in tutorial mode to decide which part to display
    std::size_t courseIndex = 0; //if hosting which course/billiard table we last chose.
    std::int32_t ballSkinIndex = 0; //billiards balls
    std::int32_t tableSkinIndex = 0; //billiards table

    //IDs used by the unlock state to display new unlocks
    //and optional XP value to display (0 is ignored)
    struct Unlock final
    {
        std::int32_t id = 0;
        std::int32_t xp = 0;
    };
    std::vector<Unlock> unlockedItems;

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
    bool useSwingput = false;
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
    bool showPuttingPower = true;
    bool showBallTrail = false;
    bool trailBeaconColour = true; //if false defaults to white
    bool fastCPU = true;
    std::int32_t enableRumble = 1;
    std::int32_t clubSet = 0;
    std::int32_t preferredClubSet = 0; //this is what the player chooses, may be overridden by game rules
    std::int32_t crowdDensity = 1;
    std::int32_t groupMode = 0;
    bool pressHold = false; //press and hold the action button to select power
    bool useTTS = false;
    bool useLensFlare = true;
    bool useMouseAction = true;
    bool useLargePowerBar = false;
    bool useContrastPowerBar = false;
    bool decimatePowerBar = false;
    bool decimateDistance = false;
    bool showRosterTip = true;
    bool fixedPuttingRange = false;
    std::int32_t lightmapQuality = 0;
    
    bool webSocket = false;
    std::int32_t webPort = 8080;
    bool logCSV = false;
    bool blockChat = false;
    bool logChat = false;
    bool remoteContent = false;
    std::int32_t flagText = 0; //none, black, white
    std::string flagPath;

    std::int32_t baseState = 0; //used to tell which state we're returning to from errors etc
    std::unique_ptr<cro::ResourceCollection> sharedResources; //globally shared resources, eg fonts
    cro::ResourceCollection* activeResources = nullptr; //used by cached states to share the base state resources
    
    std::vector<glm::uvec2> resolutions;
    std::vector<std::string> resolutionStrings;

    LeagueNames leagueNames;
    std::array<Tournament, 2u> tournaments;
    std::int32_t activeTournament = -1;
};
