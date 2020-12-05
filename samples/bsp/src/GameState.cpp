/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "GameState.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/Keyboard.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/Sunlight.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ReflectionMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/CircleMeshBuilder.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
#include "SeaShader.inl"

    cro::Entity moveEnt;

    const float SeaRadius = 50.f;
    const float CameraHeight = 2.f;
    const std::uint32_t ReflectionMapSize = 512;

    glm::vec3 LightColour = glm::vec3(1.0);
    glm::vec3 LightRotation = glm::vec3(-cro::Util::Const::PI * 0.9f, 0.f, 0.f);

    float ShadowmapProjection = 80.f;
    float ShadowmapClipPlane = 100.f;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_waterIndex    (0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
        });

#ifdef CRO_DEBUG_

    registerWindow([&]()
        {
            if (ImGui::Begin("Buns"))
            {
                if (ImGui::SliderFloat("Shadow Map Projection", &ShadowmapProjection, 10.f, 200.f))
                {
                    auto half = ShadowmapProjection / 2.f;
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setProjectionMatrix(glm::ortho(-half, half, -half, half, 0.1f, ShadowmapClipPlane));
                }

                if (ImGui::SliderFloat("Shadow Map Far Plane", &ShadowmapClipPlane, 1.f, 500.f))
                {
                    auto half = ShadowmapProjection / 2.f;
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setProjectionMatrix(glm::ortho(-half, half, -half, half, 0.1f, ShadowmapClipPlane));
                }

                if (ImGui::ColorEdit3("Light Colour", &LightColour[0]))
                {
                    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setColour(cro::Colour(LightColour.r, LightColour.g, LightColour.b, 1.f));
                }

                if (ImGui::SliderFloat3("Light Rotation", &LightRotation[0], -cro::Util::Const::PI, cro::Util::Const::PI))
                {
                    m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, LightRotation.x);
                    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, LightRotation.y);
                    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, LightRotation.z);
                }

                if (ImGui::CollapsingHeader("ShadowMap"))
                {
                    ImGui::Image(m_gameScene.getSystem<cro::ShadowMapRenderer>().getDepthMapTexture(),
                        { 320.f, 320.f }, { 0.f, 0.f }, { 1.f, 1.f });
                }

                if (ImGui::CollapsingHeader("Reflection Map"))
                {
                    const auto& reflectionMapRenderer = m_gameScene.getSystem<cro::ReflectionMapRenderer>();
                    ImGui::Image(reflectionMapRenderer.getReflectionTexture(),
                        { 320.f, 320.f }, { 0.f, 0.f }, { 1.f, 1.f });

                    ImGui::Image(reflectionMapRenderer.getRefractionTexture(),
                        { 320.f, 320.f }, { 0.f, 0.f }, { 1.f, 1.f });
                }
            }
            ImGui::End();
        });

#endif
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GameState::simulate(float dt)
{
    glm::vec3 movement(0.f);
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_D))
    {
        movement.x += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_A))
    {
        movement.x -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_W))
    {
        movement.z -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_S))
    {
        movement.z += 1.f;
    }
    if (glm::length2(movement) > 0)
    {
        movement = glm::normalize(movement);
    }
    const float Speed = 10.f;
    moveEnt.getComponent<cro::Transform>().move(movement * Speed * dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    auto& rt = cro::App::getWindow();
    m_gameScene.render(rt);
    m_uiScene.render(rt);
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ReflectionMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void GameState::loadAssets()
{
    m_resources.shaders.loadFromString(ShaderID::Sea, SeaVertex, SeaFragment);
    m_materialIDs[MaterialID::Sea] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Sea));

    m_environmentMap.loadFromFile("assets/images/cubemap/beach02.hdr");

    //m_gameScene.setCubemap(m_environmentMap);
    m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI * 0.9f);

    //m_resources.textures.load(TextureID::SeaNormal, "assets/images/water_normal.png", true);
    //auto& waterNormal = m_resources.textures.get(TextureID::SeaNormal);
    //waterNormal.setRepeated(true);
    //waterNormal.setSmooth(true);
    //m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_normalMap", waterNormal);

    const std::string basePath = "assets/images/water/water (";
    for (auto i = 0u; i < m_waterTextures.size(); ++i)
    {
        auto path = basePath + std::to_string((i * 3) + 1) + ").png";
        if (!m_waterTextures[i].loadFromFile(path, true))
        {
            LogW << "Failed to load " << path << std::endl;
        }
        else
        {
            m_waterTextures[i].setSmooth(true);
            m_waterTextures[i].setRepeated(true);

        }
    }
    m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_normalMap", m_waterTextures[m_waterIndex]);
}

void GameState::createScene()
{
    //sea plane
    auto gridID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(SeaRadius, 30));

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -SeaRadius });
    entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(gridID), m_resources.materials.get(m_materialIDs[MaterialID::Sea]));

    //TODO add this to some sort of system for updating sea properties
    //TODO update material properties with uniform ID setter to save string lookups
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto pos = e.getComponent<cro::Transform>().getWorldPosition();
        pos /= (SeaRadius * 2.f);
        e.getComponent<cro::Model>().setMaterialProperty(0, "u_textureOffset", glm::vec2(pos.x, -pos.z));

        static float elapsed = dt;
        elapsed += dt;
        e.getComponent<cro::Model>().setMaterialProperty(0, "u_time", elapsed);

        static cro::Clock frameClock;
        if (frameClock.elapsed().asSeconds() > 1.f / 24.f)
        {
            frameClock.restart();

            m_waterIndex = (m_waterIndex + 1) % m_waterTextures.size();
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_normalMap", cro::TextureID(m_waterTextures[m_waterIndex].getGLHandle()));
        }
    };

    
    //main camera
    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&GameState::updateView, this, std::placeholders::_1);
    camEnt.getComponent<cro::Transform>().move({ 0.f, CameraHeight, 0.f });

    auto rotation = glm::lookAt(camEnt.getComponent<cro::Transform>().getPosition(), entity.getComponent<cro::Transform>().getPosition(), cro::Transform::Y_AXIS);
    camEnt.getComponent<cro::Transform>().setRotation(glm::inverse(rotation));

    //rotate the ground plane in the opposite direction to level it out again
    entity.getComponent<cro::Transform>().rotate(rotation);
    camEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //add the light source to the camera so shadow map follows
    camEnt.getComponent<cro::Transform>().addChild(m_gameScene.getSunlight().getComponent<cro::Transform>());
    m_gameScene.getSunlight().getComponent<cro::Transform>().setPosition({ 0.f, 10.f, -16.f });
    m_gameScene.getSunlight().getComponent<cro::Sunlight>().setProjectionMatrix(glm::ortho(-40.f, 40.f, -40.f, 40.f, 0.1f, 100.f));

    //TODO attach the camera to the player and move player instead.
    moveEnt = camEnt;


    

    //placeholder for player scale
    cro::ModelDefinition md;
    md.loadFromFile("assets/models/player_box.cmt", m_resources, &m_environmentMap);
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, 0.5f);// .setPosition({ 0.f, 0.85f, -6.f });
    md.createModel(entity, m_resources);

    md.loadFromFile("assets/models/arrow.cmt", m_resources);
    md.createModel(m_gameScene.getSunlight(), m_resources);

    //box to display shadow frustum
    md.loadFromFile("assets/models/frustum.cmt", m_resources);
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 0.f, 0.f, 0.5f });
    md.createModel(entity, m_resources);
    m_gameScene.getSunlight().getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float) 
    {
        e.getComponent<cro::Transform>().setScale({ ShadowmapProjection, ShadowmapProjection, ShadowmapClipPlane });
    };
}

void GameState::createUI()
{

}

void GameState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    cam3D.projectionMatrix = glm::perspective(42.f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    //update the UI camera to match the new screen size
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}