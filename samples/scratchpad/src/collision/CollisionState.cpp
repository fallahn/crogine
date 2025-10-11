/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "CollisionState.hpp"
#include "BallSystem.hpp"
#include "RollSystem.hpp"
#include "Utils.hpp"
#include "../StateIDs.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/MeshBuilder.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    bool showDebug = false;
}

CollisionState::CollisionState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus())
{
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);    


    //prevent resizing cos everything assplode otherwise :3
    m_vertexData.reserve(2);
    m_indexData.reserve(2);


    buildScene();

    registerWindow([&]() 
        {
            if (ImGui::Begin("Stats"))
            {
                ImGui::Text("P: Show debug");
                ImGui::Text("L: Hit Ball");
                ImGui::Text("Backspace: Quit");
                ImGui::Separator();

                const auto& ball = m_ballEntity.getComponent<Ball>();
                ImGui::Text("Velocity %3.3f, %3.3f, %3.3f", ball.velocity.x, ball.velocity.y, ball.velocity.z);
                ImGui::Text("Length2: %3.3f", glm::length2(ball.velocity));

                switch (ball.state)
                {
                default:break;
                case Ball::State::Awake:
                    ImGui::Text("State: Awake");
                    break;
                case Ball::State::Sleep:
                    ImGui::Text("State: Sleeping");
                    break;
                }
                
                switch (ball.currentTerrain)
                {
                default: break;
                case 0:
                    ImGui::Text("Terrain: Rough");
                    break;
                case 1:
                    ImGui::Text("Terrain: Fairway");
                    break;
                case 2:
                    ImGui::Text("Terrain: Green");
                    break;
                case 3:
                    ImGui::Text("Terrain: Bunker");
                    break;
                }
            }
            ImGui::End();        
        });
}

CollisionState::~CollisionState()
{
    for (auto& o : m_groundObjects)
    {
        m_collisionWorld->removeCollisionObject(o.get());
    }
}

//public
bool CollisionState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_l:
        {
            auto randImpulse = glm::vec3(cro::Util::Random::value(-0.05f, 0.05f), 25.f, -0.02f);

            auto& ball = m_ballEntity.getComponent<Ball>();
            ball.state = Ball::State::Awake;
            ball.velocity += randImpulse;

            auto& roller = m_rollingEntity.getComponent<Roller>();
            roller.state = Roller::Air;
            roller.velocity += randImpulse;
        }
            break;
        case SDLK_p:
            showDebug = !showDebug;
            m_debugDrawer.setDebugMode(showDebug ? std::numeric_limits<std::int32_t>::max() : 0);
            break;
        case SDLK_q:
            resetRoller();
            break;
        case SDLK_BACKSPACE:
            requestStackPop();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_scene.forwardEvent(evt);

    return false;
}

void CollisionState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool CollisionState::simulate(float dt)
{
    m_scene.simulate(dt);

    m_collisionWorld->debugDrawWorld();

    return false;
}

void CollisionState::render()
{
    m_scene.render();

    //render the debug stuff
    auto viewProj = m_scene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix;
    m_debugDrawer.render(viewProj);
}

//private
void CollisionState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<BallSystem>(mb, m_collisionWorld);
    m_scene.addSystem<RollingSystem>(mb, m_collisionWorld, m_collisionDispatcher);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);

    /*if (m_environmentMap.loadFromFile("assets/images/hills.hdr"))
    {
        m_scene.setCubemap(m_environmentMap);
    }*/
    m_environmentMap.loadFromFile("assets/images/hills.hdr");

    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/models/sphere.cmt");

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 5.f, 0.f });
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.021f, 0.f });
    md.createModel(entity);
    entity.addComponent<Ball>();
    m_ballEntity = entity;

    if (md.loadFromFile("assets/collision/models/physics_test.cmt"))
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        addCollisionMesh(entity.getComponent<cro::Model>().getMeshData(), entity.getComponent<cro::Transform>().getLocalTransform());
    }


    if (md.loadFromFile("assets/models/sphere_1m.cmt"))
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(RollResetPosition);
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.5f, 0.f });
        md.createModel(entity);
        entity.addComponent<Roller>().resetPosition = RollResetPosition;

        m_rollingEntity = entity;


        //ramp ent
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(RampResetPosition);
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.5f, 0.f });
        md.createModel(entity);
        entity.addComponent<Roller>().resetPosition = entity.getComponent<cro::Transform>().getPosition();
        entity.getComponent<Roller>().friction = 1.f;
        m_rampEntity = entity;
    }

    if (md.loadFromFile("assets/models/ramp.cmt"))
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({0.f, -10.f, -25.f});
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -85.f * cro::Util::Const::degToRad);
        md.createModel(entity);
        addCollisionMesh(entity.getComponent<cro::Model>().getMeshData(), entity.getComponent<cro::Transform>().getLocalTransform());
    }

    auto callback = [](cro::Camera& cam)
    {
        glm::vec2 winSize(cro::App::getWindow().getSize());
        cam.setPerspective(60.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, 80.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto& camera = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camera.resizeCallback = callback;
    camera.shadowMapBuffer.create(2048, 2048);
    camera.setShadowExpansion(10.f);
    callback(camera);

    m_scene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 15.f, 15.f });
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -20.f * cro::Util::Const::degToRad);

    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -25.f * cro::Util::Const::degToRad);
}

void CollisionState::addCollisionMesh(const cro::Mesh::Data& meshData, glm::mat4 groundTransform)
{
    //Future Me: if you add more meshes make sure to reserve
    //space in the vertex data arrays, sincerely Past You.

    auto& vertexData = m_vertexData.emplace_back();
    auto& indexData = m_indexData.emplace_back();
    cro::Mesh::readVertexData(meshData, vertexData, indexData);

    if ((meshData.attributeFlags & cro::VertexProperty::Colour) == 0)
    {
        LogE << "Mesh has no colour property in vertices. Ground will not be created." << std::endl;
        return;
    }

    std::int32_t colourOffset = 0;
    for (auto i = 0; i < cro::Mesh::Attribute::Colour; ++i)
    {
        colourOffset += meshData.attributes[i].size;
    }

    btTransform transform;
    transform.setFromOpenGLMatrix(&groundTransform[0][0]);

    //we have to create a specific object for each sub mesh
    //to be able to tag it with a different terrain...
    for (auto i = 0u; i < indexData.size(); ++i)
    {
        btIndexedMesh groundMesh;
        groundMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(vertexData.data());
        groundMesh.m_numVertices = meshData.vertexCount;
        groundMesh.m_vertexStride = meshData.vertexSize;

        groundMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        groundMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(indexData[i].data());
        groundMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);

        
        float terrain = vertexData[(indexData[i][0] * (meshData.vertexSize / sizeof(float))) + colourOffset] * 255.f;
        terrain = std::floor(terrain / 10.f);

        m_groundVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(groundMesh);
        m_groundShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_groundVertices.back().get(), false));
        m_groundObjects.emplace_back(std::make_unique<btPairCachingGhostObject>())->setCollisionShape(m_groundShapes.back().get());
        m_groundObjects.back()->setWorldTransform(transform);
        m_groundObjects.back()->setUserIndex(static_cast<std::int32_t>(terrain)); // set the terrain type
        m_collisionWorld->addCollisionObject(m_groundObjects.back().get());
    }
}

void CollisionState::resetRoller()
{
    m_rampEntity.getComponent<cro::Transform>().setPosition(RampResetPosition);

    auto& roller = m_rampEntity.getComponent<Roller>();
    roller.velocity = { 0.f, 0.f , 0.f };
    roller.state = Roller::Air;
}