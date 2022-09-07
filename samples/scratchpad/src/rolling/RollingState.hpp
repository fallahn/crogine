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

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <reactphysics3d/reactphysics3d.h>

#include <memory>

namespace rp = reactphysics3d;

namespace cro
{
    struct Camera;
}

class RollingState final : public cro::State, public cro::GuiClient
{
public:
    RollingState(cro::StateStack&, cro::State::Context);
    ~RollingState();

    cro::StateID getStateID() const override { return States::ScratchPad::Rolling; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::ResourceCollection m_resources;

    rp::PhysicsCommon m_physCommon;
    rp::PhysicsWorld* m_physWorld;
    rp::SphereShape* m_sphereShape;

    //if we want more than one static mesh increase this to a vector of vectors
    //as the vertex data needs to be kept around for each collision mesh object
    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;
    std::unique_ptr<rp::TriangleVertexArray> m_triangleVerts;

    cro::Entity m_ballEnt;
    cro::Entity m_debugMesh;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void parseStaticMesh(cro::Entity);
    void resetBall();
};