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

#include "RollingState.hpp"
#include "RollingSystem.hpp"
#include "Utility.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct ShaderID final
    {
        enum
        {
            Wireframe,

            Count
        };
    };
    std::array<std::int32_t, ShaderID::Count> ShaderIDs = {};

    struct MaterialID final
    {
        enum
        {
            Wireframe,

            Count
        };
    };

    std::array<std::int32_t, MaterialID::Count> MaterialIDs = {};

    glm::vec3 debugPos(0.f);
    float debugMaxHeight = 0.f;
    float debugVelocity = 0.f;
}

RollingState::RollingState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_physWorld     (nullptr),
    m_sphereShape   (nullptr),
    m_ballDef       (m_resources)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

RollingState::~RollingState()
{
    //I believe this is all RAII - but experience tells me not to trust
    m_physCommon.destroyPhysicsWorld(m_physWorld);
}

//public
bool RollingState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_q:
            //resetBall();
            spawnBall();
            break;
        case SDLK_p:
            m_physWorld->setIsDebugRenderingEnabled(!m_physWorld->getIsDebugRenderingEnabled());
            m_debugMesh.getComponent<cro::Model>().setHidden(!m_physWorld->getIsDebugRenderingEnabled());
            break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);

    return true;
}

void RollingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
}

bool RollingState::simulate(float dt)
{
    if (m_camController.getComponent<cro::Callback>().getUserData<cro::Entity>().isValid())
    {
        rp::Vector3 force(0.f, 0.f, 0.f);
        if (cro::Keyboard::isKeyPressed(SDLK_UP))
        {
            force.z -= 1000.f;
        }

        if (force.lengthSquare() > 0)
        {
            auto ball = m_camController.getComponent<cro::Callback>().getUserData<cro::Entity>();
            auto* body = ball.getComponent<cro::Callback>().getUserData<rp::RigidBody*>();
            
            debugVelocity = body->getLinearVelocity().length();
            if (debugVelocity < 5)
            {
                body->applyWorldForceAtCenterOfMass(force);
            }
        }
    }

    m_physWorld->update(dt);

    if (m_physWorld->getIsDebugRenderingEnabled())
    {
        const auto& debug = m_physWorld->getDebugRenderer();
        const auto& lines = debug.getLines();
        const auto& tris = debug.getTriangles();

        //baahhh we have to convert the colour type
        struct Vertex final
        {
            glm::vec3 pos = glm::vec3(0.f);
            cro::Colour col;
            Vertex(glm::vec3 p, cro::Colour c)
                : pos(p), col(c) {}
        };

        std::array<std::vector<std::uint32_t>, 2> indices;
        std::uint32_t i = 0;
        std::vector<Vertex> verts;
        for (const auto& l : lines)
        {
            verts.emplace_back(toGLM(l.point1), cro::Colour(l.color1));
            indices[0].push_back(i++);
            verts.emplace_back(toGLM(l.point2), cro::Colour(l.color2));
            indices[0].push_back(i++);
        }

        for (const auto& t : tris)
        {
            //bit of a fudge but approximates rendering as LINES not tris
            verts.emplace_back(toGLM(t.point1), cro::Colour::Magenta);
            indices[1].push_back(i++);
            verts.emplace_back(toGLM(t.point2), cro::Colour::Magenta);
            indices[1].push_back(i++);
            indices[1].push_back(i);
            verts.emplace_back(toGLM(t.point3), cro::Colour::Magenta);
            indices[1].push_back(i++);
            indices[1].push_back(i);
            indices[1].push_back(i-3);
        }

        auto& meshData = m_debugMesh.getComponent<cro::Model>().getMeshData();
        glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.vboID);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_DYNAMIC_DRAW);

        auto* submesh = &meshData.indexData[0];
        submesh->indexCount = static_cast<std::uint32_t>(indices[0].size());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices[0].data(), GL_DYNAMIC_DRAW);

        submesh = &meshData.indexData[1];
        //submesh->primitiveType = GL_TRIANGLES;
        submesh->indexCount = static_cast<std::uint32_t>(indices[1].size());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices[1].data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    m_gameScene.simulate(dt);
    return true;
}

void RollingState::render()
{
    m_gameScene.render();

    auto oldCam = m_gameScene.setActiveCamera(m_followCam);
    m_gameScene.render();
    m_gameScene.setActiveCamera(oldCam);
}

//private
void RollingState::addSystems()
{
    //set up the physics stuff first
    m_physWorld = m_physCommon.createPhysicsWorld();
    m_physWorld->getDebugRenderer().setIsDebugItemDisplayed(rp::DebugRenderer::DebugItem::CONTACT_NORMAL, true);
    m_physWorld->getDebugRenderer().setIsDebugItemDisplayed(rp::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
    m_physWorld->getDebugRenderer().setIsDebugItemDisplayed(rp::DebugRenderer::DebugItem::COLLIDER_AABB, true);
    m_physWorld->setIsDebugRenderingEnabled(false);
    m_sphereShape = m_physCommon.createSphereShape(0.5f);

    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<RollSystem>(mb, *m_physWorld);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    ShaderIDs[ShaderID::Wireframe] = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::Mesh::Position | cro::Mesh::Colour);
    MaterialIDs[MaterialID::Wireframe] = m_resources.materials.add(m_resources.shaders.get(ShaderIDs[ShaderID::Wireframe]));
}

void RollingState::loadAssets()
{

}

void RollingState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/ramp.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -38.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 180.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        parseStaticMesh(entity);
    }

    m_ballDef.loadFromFile("assets/models/sphere_1m.cmt");

    //debug mesh
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 2, GL_LINES));
    auto material = m_resources.materials.get(MaterialIDs[MaterialID::Wireframe]);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().getMeshData().boundingBox = { glm::vec3(-20.f), glm::vec3(20.f) };
    entity.getComponent<cro::Model>().getMeshData().boundingSphere = entity.getComponent<cro::Model>().getMeshData().boundingBox;
    entity.getComponent<cro::Model>().setHidden(true);
    m_debugMesh = entity;


    auto updateView = [](cro::Camera& cam3D) 
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        size.y = ((size.x / 16.f) * 9.f) / size.y;
        size.x = 1.f;

        //90 deg in x (glm expects fov in y)
        cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 60.f);
        cam3D.viewport.bottom = (1.f - size.y) / 2.f;
        cam3D.viewport.height = size.y;
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(2048, 2048);
    camEnt.getComponent<cro::Transform>().move({ -36.f, 32.f, -26.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -20.f * cro::Util::Const::degToRad);
    updateView(camEnt.getComponent<cro::Camera>());

    m_camController = m_gameScene.createEntity();
    m_camController.addComponent<cro::Transform>();
    m_camController.addComponent<cro::Callback>().active = true;
    m_camController.getComponent<cro::Callback>().setUserData<cro::Entity>();
    m_camController.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        const auto& target = e.getComponent<cro::Callback>().getUserData<cro::Entity>();
        if (target.isValid())
        {
            e.getComponent<cro::Transform>().setPosition(target.getComponent<cro::Transform>().getPosition());
        }
    };

    auto updateFollow = [](cro::Camera& cam3D)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        size.y = ((size.x / 16.f) * 9.f) / size.y;
        size.x = 1.f;
        size /= 4.f;

        //90 deg in x (glm expects fov in y)
        cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 60.f);
        cam3D.viewport.bottom = 0.01f;
        cam3D.viewport.height = size.y;
        cam3D.viewport.left = 1.f - (size.x + 0.01f);
        cam3D.viewport.width = size.x;
    };
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setLocalTransform(glm::inverse(glm::lookAt(glm::vec3(0.f, 1.3f, 6.f), glm::vec3(0.f), cro::Transform::Y_AXIS)));
    camEnt.addComponent<cro::Camera>().resizeCallback = updateFollow;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(1024, 1024);
    updateFollow(camEnt.getComponent<cro::Camera>());
    m_camController.getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());
    m_followCam = camEnt;

    if (md.loadFromFile("assets/models/cube_025m.cmt"))
    {
        md.createModel(camEnt);
    }

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -60.f * cro::Util::Const::degToRad);
    m_gameScene.setCubemap("assets/billiards/skybox/sky.ccm");
}

void RollingState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of happiness"))
            {
                ImGui::Text("Pos: %3.3f, %3.3f, %3.3f", debugPos.x, debugPos.y, debugPos.z);
                ImGui::Text("Max Height: %3.3f", debugMaxHeight);
                ImGui::Text("Velocity: %3.3f", debugVelocity);
            }
            ImGui::End();        
        });
}

void RollingState::parseStaticMesh(cro::Entity entity)
{
    //TODO this only supports one mesh and the first sub-mesh
    //needs updating if the model changes or more models are added.

    const auto& meshData = entity.getComponent<cro::Model>().getMeshData();
    cro::Mesh::readVertexData(meshData, m_vertexData, m_indexData);

    //this makes my whiskers itch.
    m_triangleVerts = std::make_unique<rp::TriangleVertexArray>(
        static_cast<std::uint32_t>(m_vertexData.size()),
        (void*)m_vertexData.data(),
        static_cast<std::uint32_t>(meshData.vertexSize),
        static_cast<std::uint32_t>(m_indexData[0].size() / 3),
        (void*)m_indexData[0].data(),
        static_cast<std::uint32_t>(3 * sizeof(std::uint32_t)),
        rp::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
        rp::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE);

    auto* mesh = m_physCommon.createTriangleMesh();
    mesh->addSubpart(m_triangleVerts.get());

    auto* shape = m_physCommon.createConcaveMeshShape(mesh);

    auto pos = toR3D(entity.getComponent<cro::Transform>().getPosition());
    auto rot = toR3D(entity.getComponent<cro::Transform>().getRotation());
    auto tx = rp::Transform(pos, rot);

    //auto* body = m_physWorld->createCollisionBody(tx);
    auto* body = m_physWorld->createRigidBody(tx);
    body->setType(rp::BodyType::STATIC);
    body->addCollider(shape, rp::Transform::identity());
}

void RollingState::spawnBall()
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(BallSpawnPosition);
    m_ballDef.createModel(entity);

    rp::Transform tx(toR3D(BallSpawnPosition), rp::Quaternion::identity());

    auto* body = m_physWorld->createRigidBody(tx);
    body->addCollider(m_sphereShape, rp::Transform::identity());
    body->setMass(260.f);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<rp::RigidBody*>(body);
    entity.getComponent<cro::Callback>().function =
        [&,body](cro::Entity e, float)
    {
        auto pos = toGLM(body->getTransform().getPosition());
        auto rot = toGLM(body->getTransform().getOrientation());

        e.getComponent<cro::Transform>().setPosition(pos);
        e.getComponent<cro::Transform>().setRotation(rot);

        debugPos = pos;

        if (pos.x < 0 && pos.y > debugMaxHeight)
        {
            debugMaxHeight = pos.y;
        }

        if (pos.y < -10.f)
        {
            m_physWorld->destroyRigidBody(body);
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };

    m_camController.getComponent<cro::Callback>().setUserData<cro::Entity>(entity);
}

void RollingState::resetBall()
{
    /*auto& roller = m_ballEnt.getComponent<Roller>();
    roller.prevPosition = roller.resetPosition;
    roller.state = Roller::Air;
    roller.velocity = glm::vec3(0.f);
    roller.prevVelocity = roller.velocity;

    m_ballEnt.getComponent<cro::Transform>().setPosition(roller.resetPosition);*/
    

}