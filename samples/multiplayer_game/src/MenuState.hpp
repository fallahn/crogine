/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "StateIDs.hpp"
#include "CommonConsts.hpp"

#include <crogine/audio/sound_system/SoundRecorder.hpp>
#include <crogine/audio/DynamicAudioStream.hpp>

#include <crogine/core/State.hpp>
#include <crogine/core/String.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/MeshResource.hpp>
#include <crogine/graphics/ShaderResource.hpp>
#include <crogine/graphics/MaterialResource.hpp>
#include <crogine/graphics/TextureResource.hpp>
#include <crogine/graphics/Font.hpp>

#include <array>

namespace MenuCommandID
{
    enum
    {
        RootNode = 0x1,
        ReadyButton = 0x2,
        LobbyList = 0x4
    };
}

namespace GroupID
{
    enum
    {
        Main = 0,
        Avatar,
        Join,
        Lobby,
        Options
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
    ~MenuState() = default;

    cro::StateID getStateID() const override { return States::MainMenu; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    cro::SoundRecorder m_soundRecorder;
    cro::DynamicAudioStream m_audioStream;
    SharedStateData& m_sharedData;

    cro::Scene m_scene;
    cro::MeshResource m_meshResource;
    cro::ShaderResource m_shaderResource;
    cro::MaterialResource m_materialResource;
    cro::TextureResource m_textureResource;

    cro::Font m_font;
    bool m_hosting;
    std::array<bool, ConstVal::MaxClients> m_readyState = {};

    enum MenuID
    {
        Main, Avatar, Join, Lobby, Options, Count
    };

    static const std::array<glm::vec2, MenuID::Count> m_menuPositions;

    struct TextEdit final
    {
        cro::String* string = nullptr;
        cro::Entity entity;
    }m_textEdit;

    void addSystems();
    void loadAssets();
    void createScene();

    void registerUI();

    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createOptionsMenu(cro::Entity, std::uint32_t, std::uint32_t);

    void handleTextEdit(const cro::Event&);
    void applyTextEdit();
    void updateLobbyData(const cro::NetEvent&);
    void updateLobbyStrings();
    void updateReadyDisplay();

    void handleNetEvent(const cro::NetEvent&);
    void updateView(cro::Camera&);
};