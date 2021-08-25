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

#include <crogine/core/State.hpp>
#include <crogine/core/String.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

#include <array>

namespace GroupID
{
    enum
    {
        Main = 0,
        Avatar,
        Join,
        Lobby,
        PlayerConfig
    };
}

struct SharedStateData;
namespace cro
{
    struct NetEvent;
    struct Camera;
}

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
        Main, Avatar, Join, Lobby, Options, PlayerConfig, Count
    };

private:

    SharedStateData& m_sharedData;

    cro::Scene m_uiScene;
    cro::Scene m_backgroundScene;
    cro::ResourceCollection m_resources;

    cro::RenderTexture m_postBuffer;
    cro::SimpleQuad m_postQuad;
    cro::Shader m_postShader;

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
    std::array<cro::Entity, MenuID::Count> m_menuEntities = {};   
    std::vector<cro::Entity> m_avatarListEntities;
    PlayerAvatar m_playerAvatar;

    std::size_t m_currentMenu;

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

    void createUI();
    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createPlayerConfigMenu(std::uint32_t, std::uint32_t);

    void handleTextEdit(const cro::Event&);
    void applyTextEdit();
    void updateLocalAvatars(std::uint32_t, std::uint32_t);
    void updateLobbyData(const cro::NetEvent&);
    void updateLobbyAvatars();
    void showPlayerConfig(bool, std::uint8_t);

    void saveAvatars();
    void loadAvatars();

    void handleNetEvent(const cro::NetEvent&);
};