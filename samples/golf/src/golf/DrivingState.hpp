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
#include "CameraFollowSystem.hpp"
#include "InputParser.hpp"
#include "HoleData.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <crogine/detail/glm/vec2.hpp>

struct SharedStateData;
class DrivingState final : public cro::State, public cro::GuiClient
{
public:
    DrivingState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return StateID::DrivingRange; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedStateData& m_sharedData;
    InputParser m_inputParser;

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    glm::vec2 m_viewScale;

    cro::ResourceCollection m_resources;
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

    std::array<cro::Entity, CameraID::Count> m_cameras = {};


    std::vector<HoleData> m_holeData;


    cro::Entity m_defaultCam;
    cro::Entity m_freeCam;
    void toggleFreeCam();

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();
    void createPlayer(cro::Entity);

    struct TransitionData final
    {
        cro::Entity courseEnt;
    }m_transitionData;
    void startTransition();
    void updateWindDisplay(glm::vec3);

    struct SpriteID final
    {
        enum
        {
            PowerBar,
            PowerBarInner,
            HookBar,
            WindIndicator,
            MessageBoard,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};
};