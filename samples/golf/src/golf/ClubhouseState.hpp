/*-----------------------------------------------------------------------

Matt Marchant 2022
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
#include "MenuCallbacks.hpp"
#include "CommonConsts.hpp"
#include "Billboard.hpp"
#include "BilliardsSystem.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <vector>

namespace cro
{
    struct NetEvent;
}

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

struct SharedStateData;
class ClubhouseState final : public cro::State, public cro::GuiClient
{
public:
    ClubhouseState(cro::StateStack&, cro::State::Context, SharedStateData&);

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

            Count
        };
    };

private:

    SharedStateData& m_sharedData;

    cro::Scene m_backgroundScene;
    cro::Scene m_uiScene;
    cro::Scene m_tableScene;

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

    struct MaterialID final
    {
        enum
        {
            Cel,
            Trophy,
            Shelf,
            Billboard,
            Ball,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};
    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_backgroundTexture;
    cro::UniformBuffer m_scaleBuffer;
    cro::UniformBuffer m_resolutionBuffer;
    cro::UniformBuffer m_windBuffer;
    cro::CubemapTexture m_reflectionMap;

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
    }m_tableSelectCallbacks;

    void addSystems();
    void loadResources();
    void validateTables();
    void buildScene();
    void createTableScene();
    void createUI();

    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);

    void updateLobbyData(const cro::NetEvent&);
    void quitLobby();

    void beginTextEdit(cro::Entity, cro::String*, std::size_t);
    void handleTextEdit(const cro::Event&);
    bool applyTextEdit(); //returns true if this consumed event
    void addTableSelectButtons();
    void updateLobbyAvatars();
    void updateBallTexture();

    void handleNetEvent(const cro::NetEvent&);

    friend struct ClubhouseContext;
};