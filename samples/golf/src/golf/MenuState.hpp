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

#include "../StateIDs.hpp"
#include "CommonConsts.hpp"
#include "GameConsts.hpp"
#include "PlayerAvatar.hpp"
#include "Billboard.hpp"
#include "SharedStateData.hpp"
#include "MenuCallbacks.hpp"

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
#include <crogine/graphics/UniformBuffer.hpp>

#include <array>

namespace cro
{
    struct NetEvent;
    struct Camera;
}

class MenuState;
struct MainMenuContext final : public MenuContext
{
    explicit MainMenuContext(MenuState*);
};

class MenuState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    MenuState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return StateID::Menu; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

    enum MenuID
    {
        Dummy, Main, Avatar, Join, Lobby, PlayerConfig, ConfirmQuit, Count
    };

private:
    SharedStateData& m_sharedData;
    MatchMaking m_matchMaking;
    cro::Cursor m_cursor;
    cro::ResourceCollection m_resources;

    cro::Scene m_uiScene;
    cro::Scene m_backgroundScene;
    cro::Scene m_avatarScene;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,

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
            Ground,

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
    cro::Entity m_avatarMenu; //root of the avatar menu to which each player avatar is attached
    std::vector<cro::Entity> m_avatarListEntities;
    std::pair<std::uint32_t, std::uint32_t> m_avatarCallbacks;
    std::array<std::uint32_t, 5u> m_cpuOptionCallbacks = {};
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
        std::uint32_t selected = 0;
        std::uint32_t unselected = 0;
        std::uint32_t showTip = 0;
        std::uint32_t hideTip = 0;
    }m_courseSelectCallbacks;
    std::array<std::uint32_t, 4u> m_avatarEditCallbacks = {};

    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    std::size_t m_currentMenu; //used by view callback to reposition the root node on window resize
    std::size_t m_prevMenu; //used to resore active menu when completing text entry
    std::array<cro::Entity, MenuID::Count> m_menuEntities = {}; //each menu transform, attatched to root node.

    //hack to quit the lobby confirm menu from event input
    std::function<void()> enterConfirmCallback;
    std::function<void()> quitConfirmCallback;

    struct TextEdit final
    {
        cro::String* string = nullptr;
        cro::Entity entity;
        std::size_t maxLen = ConstVal::MaxStringChars;
    }m_textEdit;

    glm::vec2 m_viewScale;

    struct ControllerCallbackID final
    {
        enum
        {
            EnterLeft, ExitLeft,
            EnterRight, ExitRight,
            //this order is IMPORTANT
            Inc01,
            Inc02,
            Inc03,
            Inc04,

            Dec01,
            Dec02,
            Dec03,
            Dec04,

            Count
        };
    };
    std::array<std::uint32_t, ControllerCallbackID::Count> m_controllerCallbackIDs = {};

    void addSystems();
    void loadAssets();
    void createScene();
    void createClouds();

    struct CourseData final
    {
        cro::String directory;
        cro::String title = "Untitled";
        cro::String description = "No Description"; 
        std::array<cro::String, 3u> holeCount = {};
    };
    std::vector<CourseData> m_courseData;
    void parseCourseDirectory();

    cro::Entity m_toolTip;
    void createToolTip();
    void showToolTip(const std::string&);
    void hideToolTip();


    //----ball, avatar and hair funcs are in MenuCustomisation.cpp----//
    std::array<std::size_t, ConnectionData::MaxPlayers> m_ballIndices = {}; //index into the model list, not ballID
    cro::Entity m_ballCam;
    cro::RenderTexture m_ballTexture;
    void createBallScene();
    std::int32_t indexFromBallID(std::uint32_t);

    std::vector<PlayerAvatar> m_playerAvatars;
    //this is the index for each player into m_playerAvatars - skinID is read from PlayerAvatar struct
    std::array<std::size_t, ConnectionData::MaxPlayers> m_avatarIndices = {};
    std::array<cro::RenderTexture, ConnectionData::MaxPlayers> m_avatarThumbs = {};
    std::uint8_t m_activePlayerAvatar; //which player is current editing their avatar
    cro::RenderTexture m_avatarTexture;
    void parseAvatarDirectory();
    void createAvatarScene();
    std::int32_t indexFromAvatarID(std::uint32_t);
    void applyAvatarColours(std::size_t);
    void setPreviewModel(std::size_t);
    void updateThumb(std::size_t);

    
    //index into hair model vector - converted from hairID with indexFromHairID
    std::array<std::size_t, ConnectionData::MaxPlayers> m_hairIndices = {};
    std::int32_t indexFromHairID(std::uint32_t);


    void createUI();
    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createPlayerConfigMenu();

    //message handlers for completing connection
    void finaliseGameCreate();
    void finaliseGameJoin();

    void beginTextEdit(cro::Entity, cro::String*, std::size_t);
    void handleTextEdit(const cro::Event&);
    bool applyTextEdit(); //returns true if this consumed event
    void updateLocalAvatars(std::uint32_t, std::uint32_t);
    void updateLobbyData(const net::NetEvent&);
    void updateLobbyAvatars();
    void showPlayerConfig(bool, std::uint8_t);
    void quitLobby();
    void addCourseSelectButtons();

    //loading moved to GolfGame.cpp

    void handleNetEvent(const net::NetEvent&);

    friend struct MainMenuContext;
};