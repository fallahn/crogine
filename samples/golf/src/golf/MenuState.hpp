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

#include "../StateIDs.hpp"
#include "CommonConsts.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "PlayerAvatar.hpp"
#include "Billboard.hpp"
#include "SharedStateData.hpp"
#include "MenuCallbacks.hpp"
#include "TextChat.hpp"

#include <MatchMaking.hpp>

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/Cursor.hpp>
#include <crogine/core/State.hpp>
#include <crogine/core/String.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/VideoPlayer.hpp>

#include <array>
#include <unordered_map>

static const std::uint32_t BallRenderFlags = (1 << 22);

namespace cro
{
    struct NetEvent;
    struct Camera;
    class SpriteSheet;
}

class MenuState;
struct SharedProfileData;
struct MainMenuContext final : public MenuContext
{
    explicit MainMenuContext(MenuState*);
};

class MenuState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    MenuState(cro::StateStack&, cro::State::Context, SharedStateData&, SharedProfileData&);
    ~MenuState();
    MenuState(const MenuState&) = delete;
    MenuState(MenuState&&) = delete;
    const MenuState& operator = (const MenuState&) = delete;
    MenuState& operator = (MenuState&&) = delete;

    cro::StateID getStateID() const override { return StateID::Menu; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

    enum MenuID
    {
        Dummy, 
        Main, Avatar, Join, Lobby,
        ProfileFlyout, ConfirmQuit,
        Scorecard,

        Count
    };

private:
    SharedStateData& m_sharedData;
    SharedProfileData& m_profileData;
    std::int32_t m_connectedClientCount;
    std::int32_t m_connectedPlayerCount;

    TextChat m_textChat;
    MatchMaking m_matchMaking;
    cro::Cursor m_cursor;
    cro::ResourceCollection m_resources;

    cro::Scene m_uiScene;
    cro::Scene m_backgroundScene;
    cro::Scene m_avatarScene;
    cro::CubemapTexture m_reflectionMap;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back, Start,
            Message, Nope, Poke,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    cro::RenderTexture m_backgroundTexture;
    struct MaterialID final
    {
        enum
        {
            Cel,
            CelTextured,
            CelTexturedSkinned,
            Hair,
            Billboard,
            BillboardShadow,
            Ground,
            Trophy,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};
    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<WindData> m_windBuffer;

    std::array<bool, ConstVal::MaxClients> m_readyState = {};

    static const std::array<glm::vec2, MenuID::Count> m_menuPositions;
    float m_lobbyExpansion; //how much the lobby menu has been stretched to fill the screen in total
    cro::Entity m_avatarMenu; //root of the avatar menu to which each player avatar is attached
    std::vector<cro::Entity> m_avatarListEntities;
    std::pair<std::uint32_t, std::uint32_t> m_avatarCallbacks;
    std::array<std::uint32_t, 5u> m_cpuOptionCallbacks = {};
    std::array<std::uint32_t, 2u> m_ballPreviewCallbacks = {};
    struct HostOptionCallbacks final
    {
        std::uint32_t prevRules = 0;
        std::uint32_t nextRules = 0;
        std::uint32_t prevRadius = 0;
        std::uint32_t nextRadius = 0;
        std::uint32_t prevCourse = 0;
        std::uint32_t nextCourse = 0;
        std::uint32_t prevHoleCount = 0;
        std::uint32_t nextHoleCount = 0;
        std::uint32_t prevHoleType = 0;
        std::uint32_t nextHoleType = 0;
        std::uint32_t toggleReverseCourse = 0;
        std::uint32_t toggleClubLimit = 0;
        std::uint32_t toggleNightTime = 0;
        std::uint32_t toggleFriendsOnly = 0;
        std::uint32_t toggleFastCPU = 0;
        std::uint32_t setWeather = 0;
        std::uint32_t inviteFriends = 0;
        std::uint32_t selected = 0;
        std::uint32_t unselected = 0;
        std::uint32_t selectHighlight = 0;
        std::uint32_t unselectHighlight = 0;
        std::uint32_t selectText = 0;
        std::uint32_t unselectText = 0;
        std::uint32_t selectPM = 0;
        std::uint32_t unselectPM = 0;
        std::uint32_t activatePM = 0;
    }m_courseSelectCallbacks;
    std::array<std::uint32_t, 4u> m_avatarEditCallbacks = {};

    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    std::size_t m_currentMenu; //used by view callback to reposition the root node on window resize
    std::size_t m_prevMenu; //used to resore active menu when completing text entry
    std::array<cro::Entity, MenuID::Count> m_menuEntities = {}; //each menu transform, attatched to root node.

    struct LobbyEntityID final
    {
        enum
        {
            HoleSelection,
            HoleThumb,
            CourseTicker,
            Background,
            Info,
            MonthlyCourse,
            MinPlayerCount,

            Scorecard,

            Count
        };
    };
    std::array<cro::Entity, LobbyEntityID::Count> m_lobbyWindowEntities = {};

    LobbyPager m_lobbyPager;

    //hack to quit the lobby confirm menu from event input
    std::function<void(bool)> enterConfirmCallback;
    std::function<void()> quitConfirmCallback;

    struct TextEdit final
    {
        cro::String* string = nullptr;
        cro::Entity entity;
        std::size_t maxLen = ConstVal::MaxStringChars;
    }m_textEdit;

    glm::vec2 m_viewScale;

    void addSystems();
    void loadAssets();
    void createScene();
    void createClouds();

    struct CourseData final
    {
        cro::String directory;
        cro::String title = "Untitled";
        cro::String description = "No Description";
        std::int32_t courseNumber = 0; //base 1
        std::array<cro::String, 3u> holeCount = {};
        std::vector<std::int32_t> parVals;
        bool isUser = false;
    };
    std::vector<CourseData> m_courseData;
    std::unordered_map<std::string, std::unique_ptr<cro::Texture>> m_courseThumbs;
    std::unordered_map<std::string, std::string> m_videoPaths;
    cro::VideoPlayer m_videoPlayer;
    
    struct Range final
    {
        std::size_t start = 0;
        std::size_t count = 0;

        enum
        {
            Official, Custom, Workshop,
            Count
        };
    };
    std::int32_t m_currentRange = Range::Official;
    std::array<Range, Range::Count> m_courseIndices = {};

    void parseCourseDirectory(const std::string&, bool isUser);

    cro::Entity m_toolTip;
    void createToolTip();
    void showToolTip(const std::string&);
    void hideToolTip();


    //----ball, avatar and hair funcs are in MenuCustomisation.cpp----//
    cro::Entity m_ballCam;
    cro::RenderTexture m_ballTexture;
    void createBallScene();
    std::int32_t indexFromBallID(std::uint32_t);

    /*
    OK Since the avatar update this has gotten into a bit of a mess:
    PlayerAvatar represents each one of the avatar models and its texture. (size n where n is number of models available)
    ProfileTexture represents the texture and mugshot texture for each avatar used when previewing profiles in the
    roster menu, so is size 1-MaxPlayers
    HOWEVER as each profile requres a uniqe texture, but multiple profiles may use the SAME avatar then m_playerAvatars
    (holding the avatar textures) is update with colour settings, but those colours are then applied to m_profileTextures[x]

    On the face of it, it would seem a simple refactor to remove the unused image/colour information from ProfileTexture
    and the unused texture from PlayerAvatar - if it weren't for the fact PlayerAvatar *inherits* ProfileTexture

    I apologise profusely for this mobius strip of madness.
    */
    std::vector<PlayerAvatar> m_playerAvatars;
    std::vector<ProfileTexture> m_profileTextures;
    void updateProfileTextures(std::size_t start, std::size_t count); //applies profile colours to each texture

    std::function<void()> updateRoster; //refreshes the roster list / avatar preview
    
    cro::RenderTexture m_avatarTexture;
    void parseAvatarDirectory();
    void processAvatarList(const std::vector<std::string>&, const std::string&, const std::string = "");
    void createAvatarScene();
    std::int32_t indexFromAvatarID(std::uint32_t);
    void ugcInstalledHandler(std::uint64_t id, std::int32_t type);

    cro::RenderTexture m_clubTexture;
    std::int32_t indexFromHairID(std::uint32_t);

    struct Roster final
    {
        std::size_t activeIndex = 0;
        std::array<std::size_t, ConstVal::MaxPlayers> profileIndices = {}; //the index of the profile at this slot
        std::array<cro::Entity, ConstVal::MaxPlayers> selectionEntities = {}; //entity to show active slot
        std::array<cro::Entity, ConstVal::MaxPlayers> buttonEntities = {}; //button to select slot
    }m_rosterMenu;

    FlyoutMenu m_profileFlyout;

    void createUI();
    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createBrowserMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createMenuCallbacks();
    void createProfileLayout(cro::Entity, cro::Transform&, const cro::SpriteSheet&);//display XP, clubs, streak etc on avatar menu
    void eraseCurrentProfile();
    void setProfileIndex(std::size_t);
    void refreshProfileFlyout();

    //allows updating the target indices
    //of lobby buttons based on whether the
    //user is hosting or not
    struct LobbyButtonContext final
    {
        cro::Entity infoLeague;
        cro::Entity lobbyCourseA;
        cro::Entity lobbyCourseB;
        cro::Entity lobbyInfoA;
        cro::Entity lobbyInfoB;
        cro::Entity lobbyRulesA;
        cro::Entity lobbyRulesB;
        cro::Entity rulesClubset;
        bool hasScoreCard = false;
    }m_lobbyButtonContext;
    void refreshLobbyButtons();

    //message handlers for completing connection
    void finaliseGameCreate(const MatchMaking::Message&);
    void finaliseGameJoin(const MatchMaking::Message&);

    void beginTextEdit(cro::Entity, cro::String*, std::size_t);
    void handleTextEdit(const cro::Event&);
    bool applyTextEdit(); //returns true if this consumed event
    void updateLobbyData(const net::NetEvent&);
    void updateLobbyAvatars();
    void updateLobbyList();
    void quitLobby();
    void addCourseSelectButtons();
    void prevHoleCount();
    void nextHoleCount();
    void prevCourse();
    void nextCourse();
    void refreshUI();
    void updateCourseRuleString(bool updateScoreboard );
    void updateUnlockedItems();

    void createPreviousScoreCard();
    void togglePreviousScoreCard();

    void handleNetEvent(const net::NetEvent&);

    friend struct MainMenuContext;
};