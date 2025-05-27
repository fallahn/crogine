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

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

#include <functional>

namespace cro
{
    class SpriteSheet;
}

struct SharedStateData;

class TournamentState final : public cro::State
{
public:
    TournamentState(cro::StateStack&, cro::State::Context, SharedStateData&);
    ~TournamentState();

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Tournament; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back, Switch,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    cro::Entity m_playerName;
    cro::RenderTexture m_clubTexture;

    cro::Texture m_bracketTexture;
    cro::SimpleText m_treeText;
    cro::SimpleQuad m_treeQuad;
    cro::RenderTexture m_treeTexture;

    cro::Entity m_titleString;
    cro::Entity m_detailString;
    cro::Entity m_warningString;
    cro::Entity m_treeRoot;

    float m_axisPosition; //tracks right stick for tree scrolling

    struct SettingsDetails final
    {
        cro::Entity gimme;
        cro::Entity night;
        cro::Entity clubset;
    }m_settingsDetails;

    std::size_t m_currentMenu;
    std::function<void(bool, bool)> enterConfirmCallback;
    std::function<void()> quitConfirmCallback;

    std::function<void()> enterInfoCallback;
    std::function<void()> quitInfoCallback;

    std::function<void()> enterStatCallback;
    std::function<void()> quitStatCallback;

    void loadAssets();
    void addSystems();
    void buildScene();
    void createConfirmMenu(cro::Entity);
    void createInfoMenu(cro::Entity);
    void createStatMenu(cro::Entity);
    void createProfileLayout(cro::Entity bgEnt, const cro::SpriteSheet&);

    void applySettingsValues();
    void refreshTree();
    void refreshClubsetWarning();

    void quitState();

    void loadConfig();
    void saveConfig() const;

    void onCachedPush() override;
};