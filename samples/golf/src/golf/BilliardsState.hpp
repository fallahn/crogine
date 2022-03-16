/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>

namespace cro
{
    struct NetEvent;
}

struct SharedStateData;
struct ActorInfo;
class BilliardsState final : public cro::State 
{
public:
    BilliardsState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

    cro::StateID getStateID() const override { return StateID::Billiards; }

private:

    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::ResourceCollection m_resources;

    cro::RenderTexture m_gameSceneTexture;
    cro::UniformBuffer m_scaleBuffer;
    cro::UniformBuffer m_resolutionBuffer;

    cro::ModelDefinition m_ballDefinition;

    bool m_wantsGameState;
    cro::Clock m_readyClock;

    void loadAssets();
    void addSystems();
    void buildScene();

    void handleNetEvent(const cro::NetEvent&);
    void spawnBall(const ActorInfo&);
};