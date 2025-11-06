/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include "CustomTournament.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/TextureResource.hpp>

struct SharedStateData;

class EditTournamentState final : public cro::State, public cro::GuiClient
{
public:
    EditTournamentState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::EditTournament; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;

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

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;

    struct CourseInfo final
    {
        std::string dir;
        cro::String displayName;
        cro::Texture* texture = nullptr;
    };
    std::vector<CourseInfo> m_courseInfo;
    cro::TextureResource m_textures;
    
    struct Preview final
    {
        cro::Entity thumbnail;
        cro::Entity title;
    }m_preview;

    CustomTournament m_tournamentInfo;
    std::array<std::size_t, 4u> m_tierIndices = {};

    cro::Entity m_tournamentNameEntity;

    bool m_showOSK;
    bool m_showImguiInput;
    std::string m_imguiBuffer;
    void imguiWindow();

    void buildScene();
    void loadCourseInfo();
    void updatePreview(std::size_t);
    void quitState();
    void onCachedPush() override;
};