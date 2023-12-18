/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include <crogine/core/State.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/ecs/Scene.hpp>

struct SharedStateData;

class PlayerManagementState final : public cro::State
{
public:
    PlayerManagementState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::PlayerManagement; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;
    cro::ResourceCollection m_resources;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,
            Denied,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;

    struct PlayerList final
    {
        cro::Entity text;
        std::vector<cro::Entity> buttons;
        
        //indexes into shared data
        std::uint8_t selectedClient = 0;
        std::uint8_t selectedPlayer = 0;

        cro::Entity previewAvatar;
        cro::Entity previewText;

    }m_playerList;

    struct ConfirmType final
    {
        enum
        {
            Poke, Forfeit, Kick
        };
    };
    std::int32_t m_confirmType;
    cro::Clock m_cooldownTimer;

    void buildScene();
    void refreshPlayerList();
    void refreshPreview();
    void quitState();
};