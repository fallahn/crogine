/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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
#include "MenuCallbacks.hpp"
#include "CommonConsts.hpp"
#include "GameConsts.hpp"
#include "Billboard.hpp"
#include "BilliardsSystem.hpp"

#include <MatchMaking.hpp>

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/VideoPlayer.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <vector>

#ifdef USE_GNS
namespace gns
{
    struct NetEvent;
}
namespace net = gns;
#else
namespace cro
{
    struct NetEvent;
}
namespace net = cro;
#endif

class ClubhouseState;
struct ClubhouseContext final : public MenuContext
{
    explicit ClubhouseContext(ClubhouseState*);
};

struct TableClientData final : public TableData
{
    cro::Entity previewModel;
    std::int32_t tableSkinIndex = 0;
    std::int32_t ballSkinIndex = 0;
};

struct HOFCallbackData final
{
    std::int32_t state = 0; //in,hold,out
    float progress = 0.f;
};

struct SharedStateData;
struct SharedProfileData;
class ClubhouseState final : public cro::State, public cro::GuiClient
{
public:
    ClubhouseState(cro::StateStack&, cro::State::Context, SharedStateData&, const SharedProfileData&, class GolfGame&);

    cro::StateID getStateID() const override { return StateID::Clubhouse; }

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    struct MenuID final
    {
        enum
        {
            Dummy,
            Main, PlayerSelect,
            Join, Lobby,
            HallOfFame,
            Inactive,

            Count
        };
    };

private:

    SharedStateData& m_sharedData;
    const SharedProfileData& m_profileData;
    GolfGame& m_golfGame;
    MatchMaking m_matchMaking;

    cro::Scene m_backgroundScene;
    cro::Scene m_uiScene;
    cro::Scene m_tableScene;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,
            Win,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    struct MaterialID final
    {
        enum
        {
            Cel,
            CelSkinned,
            Trophy,
            Shelf,
            Billboard,
            Ball,
            TV,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};
    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_backgroundTexture;
    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<WindData> m_windBuffer;
    cro::CubemapTexture m_reflectionMap;
    cro::VideoPlayer m_arcadeVideo;
    cro::VideoPlayer m_arcadeVideo2;
    cro::VideoPlayer m_arcadeVideo3;
    cro::RenderTexture m_pictureTexture;
    cro::SimpleQuad m_pictureQuad;

    cro::RenderTexture m_tableTexture;
    cro::CubemapTexture m_tableCubemap;
    std::vector<TableClientData> m_tableData;
    std::size_t m_tableIndex; //this is separate for just the preview as the host will have update the course index before the preview is notified.
    cro::Entity m_ballCam;
    std::array<cro::Entity, 2u> m_previewBalls = {};

    glm::vec2 m_viewScale;
    static const std::array<glm::vec2, MenuID::Count> m_menuPositions; //I've forgotten why this is static...
    std::array<cro::Entity, MenuID::Count> m_menuEntities = {};
    std::size_t m_currentMenu;
    std::size_t m_prevMenu;
    std::size_t m_gameCreationIndex;

    struct TextEdit final
    {
        cro::String* string = nullptr;
        cro::Entity entity;
        std::size_t maxLen = ConstVal::MaxStringChars;
    }m_textEdit;

    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    std::array<bool, ConstVal::MaxClients> m_readyState = {};


    struct HostOptionCallbacks final
    {
        std::uint32_t prevTable = 0;
        std::uint32_t nextTable = 0;
        std::uint32_t mouseEnter = 0;
        std::uint32_t mouseExit = 0;
        std::uint32_t selectHighlight = 0;
        std::uint32_t unselectHighlight = 0;
        std::uint32_t toggleFriendsOnly = 0;
    }m_tableSelectCallbacks;

    LobbyPager m_lobbyPager;

    static constexpr std::array<std::int32_t, 8u> ArcadeKey =
    {
        SDLK_UP,
        SDLK_UP,
        SDLK_DOWN,
        SDLK_DOWN,
        SDLK_LEFT,
        SDLK_RIGHT,
        SDLK_LEFT,
        SDLK_RIGHT,
    };
    static constexpr std::array<std::int32_t, 8u> ArcadeJoy =
    {
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    };
    std::size_t m_arcadeIndexKey;
    std::size_t m_arcadeIndexJoy;
    cro::Entity m_arcadeEnt;

    void addSystems();
    void loadResources();
    void validateTables();
    void buildScene();
    void createTableScene();
    void createUI();

    static constexpr glm::uvec2 TVPictureSize = glm::uvec2(256u, 256u);
    struct TVTopFive final
    {
        static constexpr std::uint32_t ImageWidth = 184; //default size of icon - returned icon may be smaller...
        static constexpr std::size_t MaxProfiles = 5;

        std::vector<cro::String> profileNames;
        cro::Texture profileIcons;

        cro::SimpleQuad overlay;
        cro::SimpleQuad icon;
        cro::SimpleText name;

        struct State final
        {
            enum
            {
                Idle, Scroll, TransitionOut,
                TransitionIn
            };
        };
        std::size_t index = 0;
        std::int32_t state = 0;
        float scale = 0.f;

        void addProfile(std::uint64_t);
        void update(float);
    }m_tvTopFive;


    struct LobbyButtonContext final
    {
        cro::Entity backButton;
        cro::Entity startButton;
    }m_lobbyButtonContext;

    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createBrowserMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createStatMenu(cro::Entity, std::uint32_t, std::uint32_t);

    void updateLobbyData(const net::NetEvent&);
    void updateLobbyList();
    void quitLobby();

    void beginTextEdit(cro::Entity, cro::String*, std::size_t);
    void handleTextEdit(const cro::Event&);
    bool applyTextEdit(); //returns true if this consumed event
    void addTableSelectButtons();
    void refreshLobbyButtons();
    void updateLobbyAvatars();
    void updateBallTexture();

    void handleNetEvent(const net::NetEvent&);
    void finaliseGameCreate();
    void finaliseGameJoin(const MatchMaking::Message&);

    friend struct ClubhouseContext;
};