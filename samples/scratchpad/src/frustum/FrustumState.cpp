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

#include "FrustumState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct PerspectiveDebug final
    {
        float nearPlane = 0.1f;
        float farPlane = 10.f;
        float fov = 40.f;
        float aspectRatio = 1.3f;

        void update(cro::Camera& cam)
        {
            cam.setPerspective(fov * cro::Util::Const::degToRad, aspectRatio, nearPlane, farPlane);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        }
    }perspectiveDebug;

    struct KeyID final
    {
        enum
        {
            Up, Down, Left, Right,
            CW, CCW,

            Count
        };
    };
    struct KeysetID final
    {
        enum
        {
            Cube, Light
        };
    };
    std::array keysets =
    {
        std::array<std::int32_t, KeyID::Count>({SDLK_w, SDLK_s, SDLK_a, SDLK_d, SDLK_q, SDLK_e}),
        std::array<std::int32_t, KeyID::Count>({SDLK_HOME, SDLK_END, SDLK_DELETE, SDLK_PAGEDOWN, SDLK_INSERT, SDLK_PAGEUP})
    };
}

FrustumState::FrustumState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool FrustumState::handleEvent(const cro::Event& evt)
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
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void FrustumState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool FrustumState::simulate(float dt)
{
    if (rotateEntity(m_entities[EntityID::Cube], KeysetID::Cube, dt))
    {
        auto worldMat = m_entities[EntityID::Camera].getComponent<cro::Transform>().getWorldTransform();
        const auto& corners = m_entities[EntityID::Camera].getComponent<cro::Camera>().getFrustumCorners();
        updateFrustumVis(worldMat, corners, m_entities[EntityID::CameraViz].getComponent<cro::Model>().getMeshData());
    }

    if (rotateEntity(m_entities[EntityID::Light], KeysetID::Light, dt))
    {
        m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(m_entities[EntityID::Light].getComponent<cro::Transform>().getRotation());
    }

    //do this all the time as if it were a system
    calcLightFrusta();

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void FrustumState::render()
{
    auto oldCam = m_gameScene.setActiveCamera(m_entities[EntityID::Camera]);
    m_cameraPreview.clear();
    m_gameScene.render();
    m_cameraPreview.display();

    m_gameScene.setActiveCamera(oldCam);
    m_gameScene.render();
    m_uiScene.render();
}

//private
void FrustumState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void FrustumState::loadAssets()
{
    m_cameraPreview.create(320, 240);
}

void FrustumState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/cube.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -3.f, -15.f });
        md.createModel(entity);
        m_entities[EntityID::Cube] = entity;
    }

    if (md.loadFromFile("assets/batcat/models/arrow.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 12.f, 7.f, -20.f });
        md.createModel(entity);
        m_entities[EntityID::Light] = entity;
    }

    if (md.loadFromFile("assets/batcat/models/arrow_yellow.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(glm::vec3(0.5f));
        md.createModel(entity);
        m_entities[EntityID::LightDummy] = entity;
    }

    /*if (md.loadFromFile("assets/bush/ground_plane.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -4.f, -15.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
        entity.getComponent<cro::Transform>().setScale(glm::vec3(2.f));
        md.createModel(entity);
    }*/

    auto updateView = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        auto windowSize = size;
        size.y = ((size.x / 16.f) * 9.f) / size.y;
        size.x = 1.f;

        //90 deg in x (glm expects fov in y)
        cam.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
        cam.viewport.bottom = (1.f - size.y) / 2.f;
        cam.viewport.height = size.y;
    };

    //scene camera
    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(camEnt.getComponent<cro::Camera>());

    //this is the camera on the cube which we're visualising
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = std::bind(&PerspectiveDebug::update, &perspectiveDebug, std::placeholders::_1);
    perspectiveDebug.update(camEnt.getComponent<cro::Camera>());
    m_entities[EntityID::Camera] = camEnt;

    if (m_entities[EntityID::Cube].isValid())
    {
        m_entities[EntityID::Cube].getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());
    }

    //this entity draws the camera frustum. The points are updated in world
    //space so the entity has an identity transform
    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINES));
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    auto materialID = m_resources.materials.add(m_resources.shaders.get(shaderID));
    auto material = m_resources.materials.get(materialID);




    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    //just set any bounds as long as it's in view and isn't culled...
    entity.getComponent<cro::Model>().getMeshData().boundingBox = {glm::vec3(-10.f, -10.f, -20.f), glm::vec3(10.f, 10.f, 20.f)};
    entity.getComponent<cro::Model>().getMeshData().boundingSphere = entity.getComponent<cro::Model>().getMeshData().boundingBox;
    m_entities[EntityID::CameraViz] = entity;

    //create a new mesh instance...
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLES));
    material.blendMode = cro::Material::BlendMode::Alpha;
    material.doubleSided = true;
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().getMeshData().boundingBox = { glm::vec3(-10.f, -10.f, -20.f), glm::vec3(10.f, 10.f, 20.f) };
    entity.getComponent<cro::Model>().getMeshData().boundingSphere = entity.getComponent<cro::Model>().getMeshData().boundingBox;
    m_entities[EntityID::LightViz] = entity;


    auto worldMat = camEnt.getComponent<cro::Transform>().getWorldTransform();
    const auto& corners = camEnt.getComponent<cro::Camera>().getFrustumCorners();
    updateFrustumVis(worldMat, corners, m_entities[EntityID::CameraViz].getComponent<cro::Model>().getMeshData());


    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -60.f * cro::Util::Const::degToRad);

    if (m_entities[EntityID::Light].isValid())
    {
        m_entities[EntityID::Light].getComponent<cro::Transform>().setRotation(m_gameScene.getSunlight().getComponent<cro::Transform>().getRotation());
    }

    m_gameScene.setCubemap("assets/billiards/skybox/sky.ccm");
}

void FrustumState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of happiness"))
            {
                ImGui::Image(m_cameraPreview.getTexture(), { 320.f, 240.f }, { 0.f, 1.f }, { 1.f, 0.f });

                bool doUpdate = false;
                if (ImGui::SliderFloat("Near Plane", &perspectiveDebug.nearPlane, 0.f, perspectiveDebug.farPlane -  0.1f))
                {
                    doUpdate = true;
                }
                if (ImGui::SliderFloat("Far Plane", &perspectiveDebug.farPlane, perspectiveDebug.nearPlane + 0.1f, 100.f))
                {
                    doUpdate = true;
                }
                if (ImGui::SliderFloat("FOV", &perspectiveDebug.fov, 10.f, 100.f))
                {
                    doUpdate = true;
                }
                if (ImGui::SliderFloat("Aspect", &perspectiveDebug.aspectRatio, 0.1f, 4.f))
                {
                    doUpdate = true;
                }

                if (doUpdate)
                {
                    perspectiveDebug.update(m_entities[EntityID::Camera].getComponent<cro::Camera>());

                    auto worldMat = m_entities[EntityID::Camera].getComponent<cro::Transform>().getWorldTransform();
                    const auto& corners = m_entities[EntityID::Camera].getComponent<cro::Camera>().getFrustumCorners();
                    updateFrustumVis(worldMat, corners, m_entities[EntityID::CameraViz].getComponent<cro::Model>().getMeshData());
                }

                ImGui::Text("WASDQE:\nRotate Camera");
                ImGui::NewLine();
                ImGui::Text("Home, Del, End,\nPgDn, Ins, PgUp:\nRotate Light");
            }
            ImGui::End();        
        });

    auto updateView = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_uiScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(camEnt.getComponent<cro::Camera>());
}

void FrustumState::calcLightFrusta()
{
    //frustum corners in world coords
    auto worldMat = m_entities[EntityID::Camera].getComponent<cro::Transform>().getWorldTransform();
    auto corners = m_entities[EntityID::Camera].getComponent<cro::Camera>().getFrustumCorners();

    glm::vec3 centre = glm::vec3(0.f);

    for (auto& c : corners)
    {
        c = worldMat * c;
        centre += glm::vec3(c);
    }
    centre /= corners.size();

    //position light source
    auto light = m_gameScene.getSunlight();
    auto lightDir = -light.getComponent<cro::Sunlight>().getDirection();

    auto lightPos = centre + lightDir;

    //just set this so we can visualise what's happening
    m_entities[EntityID::LightDummy].getComponent<cro::Transform>().setPosition(lightPos);
    m_entities[EntityID::LightDummy].getComponent<cro::Transform>().setRotation(light.getComponent<cro::Transform>().getRotation());

    const auto lightView = glm::lookAt(lightPos, centre, cro::Transform::Y_AXIS);

    //world coords to light space
    glm::vec3 minPos(std::numeric_limits<float>::max());
    glm::vec3 maxPos(std::numeric_limits<float>::lowest());

    for (const auto& c : corners)
    {
        const auto p = lightView * c;
        minPos.x = std::min(minPos.x, p.x);
        minPos.y = std::min(minPos.y, p.y);
        minPos.z = std::min(minPos.z, p.z);

        maxPos.x = std::max(maxPos.x, p.x);
        maxPos.y = std::max(maxPos.y, p.y);
        maxPos.z = std::max(maxPos.z, p.z);
    }

    const auto lightProj = glm::ortho(minPos.x, maxPos.x, minPos.y, maxPos.y, -maxPos.z, -minPos.z);

    std::array lightCorners =
    {
        //near
        glm::vec4(maxPos.x, maxPos.y, -maxPos.z, 1.f),
        glm::vec4(minPos.x, maxPos.y, -maxPos.z, 1.f),
        glm::vec4(minPos.x, minPos.y, -maxPos.z, 1.f),
        glm::vec4(maxPos.x, minPos.y, -maxPos.z, 1.f),
        //far
        glm::vec4(maxPos.x, maxPos.y, -minPos.z, 1.f),
        glm::vec4(minPos.x, maxPos.y, -minPos.z, 1.f),
        glm::vec4(minPos.x, minPos.y, -minPos.z, 1.f),
        glm::vec4(maxPos.x, minPos.y, -minPos.z, 1.f)
    };
    updateFrustumVis(glm::inverse(lightView), lightCorners, m_entities[EntityID::LightViz].getComponent<cro::Model>().getMeshData(), glm::vec4(1.f, 0.f, 0.f, 0.8f));
}

void FrustumState::updateFrustumVis(glm::mat4 worldMat, const std::array<glm::vec4, 8u>& corners, cro::Mesh::Data& meshData, glm::vec4 c)
{
    struct Vert final
    {
        Vert(glm::vec3 p, glm::vec4 c = glm::vec4(1.f))
            : position(p), colour(c) {}
        glm::vec3 position = glm::vec3(0.f);
        glm::vec4 colour = glm::vec4(1.f);
    };

    std::vector<Vert> vertices =
    {
        //near
        Vert(glm::vec3(worldMat * corners[0]), c),
        Vert(glm::vec3(worldMat * corners[1]), c),
        Vert(glm::vec3(worldMat * corners[2]), c),
        Vert(glm::vec3(worldMat * corners[3]), c),

        //far
        Vert(glm::vec3(worldMat * corners[4]), c),
        Vert(glm::vec3(worldMat * corners[5]), c),
        Vert(glm::vec3(worldMat * corners[6]), c),
        Vert(glm::vec3(worldMat * corners[7]), c)
    };

    static const std::vector<std::uint32_t> lineIndices =
    {
        0,1,  1,2,  2,3,  3,0,
        4,5,  5,6,  6,7,  7,4,
        0,4,  1,5,  2,6,  3,7
    };

    static const std::vector<std::uint32_t> triangleIndices =
    {
        0,1,2,  2,3,0,
        4,5,6,  4,6,7,

        1,5,6,  1,6,2,
        0,3,7,  0,7,4,


    };

    auto* indices = meshData.primitiveType == GL_LINES ? &lineIndices : &triangleIndices;

    meshData.vertexCount = vertices.size();
    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    auto& submesh = meshData.indexData[0];
    submesh.indexCount = static_cast<std::uint32_t>(indices->size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indices->data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

bool FrustumState::rotateEntity(cro::Entity entity, std::int32_t keysetID, float dt)
{
    const float rotationSpeed = dt;
    glm::vec3 rotation(0.f);
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Down]))
    {
        rotation.x -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Up]))
    {
        rotation.x += rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Right]))
    {
        rotation.y -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::Left]))
    {
        rotation.y += rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::CW]))
    {
        rotation.z -= rotationSpeed;
    }
    if (cro::Keyboard::isKeyPressed(keysets[keysetID][KeyID::CCW]))
    {
        rotation.z += rotationSpeed;
    }

    if (glm::length2(rotation) != 0
        && entity.isValid())
    {
        auto worldMat = glm::inverse(glm::mat3(entity.getComponent<cro::Transform>().getLocalTransform()));
        entity.getComponent<cro::Transform>().rotate(/*worldMat **/ cro::Transform::X_AXIS, rotation.x);
        entity.getComponent<cro::Transform>().rotate(worldMat * cro::Transform::Y_AXIS, rotation.y);
        entity.getComponent<cro::Transform>().rotate(/*worldMat **/ cro::Transform::Z_AXIS, rotation.z);

        return true;
    }
    return false;
}