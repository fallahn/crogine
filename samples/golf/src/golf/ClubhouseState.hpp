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

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/detail/glm/vec2.hpp>

namespace cro
{
    struct NetEvent;
}

class ClubhouseState;
struct ClubhouseContext final : public MenuContext
{
    explicit ClubhouseContext(ClubhouseState*);
};

struct SharedStateData;
class ClubhouseState final : public cro::State
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

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_backgroundTexture;
    cro::UniformBuffer m_scaleBuffer;
    cro::UniformBuffer m_resolutionBuffer;

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

    void addSystems();
    void loadResources();
    void buildScene();
    void createUI();

    void createMainMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createAvatarMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createJoinMenu(cro::Entity, std::uint32_t, std::uint32_t);
    void createLobbyMenu(cro::Entity, std::uint32_t, std::uint32_t);

    void quitLobby();

    void beginTextEdit(cro::Entity, cro::String*, std::size_t);
    void handleTextEdit(const cro::Event&);
    bool applyTextEdit(); //returns true if this consumed event

    void handleNetEvent(const cro::NetEvent&);

    friend struct ClubhouseContext;
};