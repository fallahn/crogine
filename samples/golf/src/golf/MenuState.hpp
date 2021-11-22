/*-----------------------------------------------------------------------

Matt Marchant 2021
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
#include "PlayerAvatar.hpp"
#include "Billboard.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/State.hpp>
#include <crogine/core/String.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <array>

struct SharedStateData;
namespace cro
{
    struct NetEvent;
    struct Camera;
}

class MenuState;
struct MenuCallback final
{
    MenuState& menuState;
    explicit MenuCallback(MenuState& m) : menuState(m) {}

    void operator()(cro::Entity, float);
};

class MenuState final : public cro::State, public cro::GuiClient
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
        Dummy, Main, Avatar, Join, Lobby, PlayerConfig, Count
    };

private:
    SharedStateData& m_sharedData;
    cro::ResourceCollection m_resources;

    cro::Scene m_uiScene;
    cro::Scene m_backgroundScene;

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

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    std::array<bool, ConstVal::MaxClients> m_readyState = {};

    static const std::array<glm::vec2, MenuID::Count> m_menuPositions;
    cro::Entity m_avatarMenu; //root of the avatar menu to which each player avatar is attached
    std::vector<cro::Entity> m_avatarListEntities;
    PlayerAvatar m_playerAvatar;
    std::pair<std::uint32_t, std::uint32_t> m_avatarCallbacks;
    std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t> m_courseSelectCallbacks;
    std::array<std::uint32_t, 4u> m_avatarEditCallbacks = {};

    struct SpriteID final
    {
        enum
        {
            Controller,
            Keyboard,
            ArrowLeft,
            ArrowRight,
            ArrowLeftHighlight,
            ArrowRightHighlight,

            ButtonBanner,
            Cursor,
            Flag,
            PrevMenu,
            NextMenu,
            AddPlayer,
            RemovePlayer,
            ReadyUp,
            StartGame,
            Connect,
            PrevCourse,
            NextCourse,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    std::size_t m_currentMenu; //used by view callback to reposition the root node on window resize
    std::size_t m_prevMenu; //used to resore active menu when completing text entry
    std::array<cro::Entity, MenuID::Count> m_menuEntities = {}; //each menu transform, attatched to root node.

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

    struct CourseData final
    {
        cro::String directory;
        cro::String title = "Untitled";
        cro::String description = "No Description"; 
        cro::String holeCount = "0";
    };
    std::vector<CourseData> m_courseData;
    std::array<std::size_t, 4u> m_ballIndices = {}; //index into the model list, not ballID
    void parseCourseDirectory();

    cro::Entity m_ballCam;
    cro::RenderTexture m_ballTexture;
    void createBallScene();
    std::int32_t indexFromBallID(std::uint8_t);

    void parseAvatarDirectory();
    std::int32_t indexFromAvatarID(std::uint8_t);

    void createUI();
    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createPlayerConfigMenu();

    void beginTextEdit(cro::Entity, cro::String*, std::size_t);
    void handleTextEdit(const cro::Event&);
    void applyTextEdit();
    void updateLocalAvatars(std::uint32_t, std::uint32_t);
    void updateLobbyData(const cro::NetEvent&);
    void updateLobbyAvatars();
    void showPlayerConfig(bool, std::uint8_t);
    void quitLobby();
    void addCourseSelectButtons();

    void saveAvatars();
    void loadAvatars();

    void handleNetEvent(const cro::NetEvent&);

    friend struct MenuCallback;
};

//used in animation callback
struct MenuData final
{
    enum
    {
        In, Out
    }direction = In;
    float currentTime = 0.f;

    std::int32_t targetMenu = MenuState::MenuID::Main;
};