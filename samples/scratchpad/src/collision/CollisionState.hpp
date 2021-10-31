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
#include "DebugDraw.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>

#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>

class CollisionState final : public cro::State, public cro::GuiClient
{
public:
    CollisionState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::MeshCollision; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_scene;
    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

    BulletDebug m_debugDrawer;
    std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btCollisionWorld> m_collisionWorld;

    cro::Entity m_ballEntity;

    btPairCachingGhostObject m_groundObject;
    std::unique_ptr<btTriangleIndexVertexArray> m_groundVertices;
    std::unique_ptr<btBvhTriangleMeshShape> m_groundShape;

    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;

    void buildScene();
    void setupCollisionWorld(const cro::Mesh::Data&);
};
