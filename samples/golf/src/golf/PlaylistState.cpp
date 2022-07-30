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

#include "PlaylistState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "TextAnimCallback.hpp"
#include "PoissonDisk.hpp"
#include "../GolfGame.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/CircleMeshBuilder.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include "../ErrorCheck.hpp"

namespace
{
#include "WaterShader.inl"
#include "TerrainShader.inl"

    const std::string SkyboxPath = "assets/golf/skyboxes/";

    constexpr float TabAreaHeight = 0.25f; //percent of screen
}

PlaylistState::PlaylistState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_skyboxScene       (ctx.appInstance.getMessageBus()),
    m_gameScene         (ctx.appInstance.getMessageBus()),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_scaleBuffer       ("PixelScale"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_windBuffer        ("WindValues"),
    m_viewScale         (2.f),
    m_skyboxIndex       (0)
{
    ctx.mainWindow.setMouseCaptured(false);

    ctx.mainWindow.loadResources(
        [this]() 
        {
            addSystems();
            loadAssets();
            buildScene();
            buildUI();
        });
}

//public
bool PlaylistState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_BACKSPACE
#ifndef CRO_DEBUG_
            || evt.key.keysym.sym == SDLK_ESCAPE
#endif
            || evt.key.keysym.sym == SDLK_p)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        if (evt.cbutton.button == cro::GameController::ButtonB
            || evt.cbutton.button == cro::GameController::ButtonStart)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);

    m_skyboxScene.forwardEvent(evt);
    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void PlaylistState::handleMessage(const cro::Message& msg)
{
    m_skyboxScene.forwardMessage(msg);
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool PlaylistState::simulate(float dt)
{
    static WindData windData;
    windData.elapsedTime += dt;
    m_windBuffer.setData(windData);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    //do this last to make sure scene cam is up to date
    const auto& srcCam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto& dstCam = m_skyboxScene.getActiveCamera().getComponent<cro::Camera>();

    dstCam.viewport = srcCam.viewport;
    dstCam.setPerspective(srcCam.getFOV(), srcCam.getAspectRatio(), 0.5f, 14.f);

    m_skyboxScene.getActiveCamera().getComponent<cro::Transform>().setRotation(
        m_gameScene.getActiveCamera().getComponent<cro::Transform>().getWorldRotation());

    m_skyboxScene.simulate(dt);
    return false;
}

void PlaylistState::render()
{
    m_scaleBuffer.bind(0);
    m_windBuffer.bind(1);
    m_resolutionBuffer.bind(2);


    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto oldVP = cam.viewport;

    cam.viewport = { 0.f,0.f,1.f,1.f };

    cam.setActivePass(cro::Camera::Pass::Reflection);
    cam.renderFlags = RenderFlags::Reflection;

    auto& skyCam = m_skyboxScene.getActiveCamera().getComponent<cro::Camera>();
    skyCam.setActivePass(cro::Camera::Pass::Reflection);
    skyCam.renderFlags = RenderFlags::Reflection;
    skyCam.viewport = { 0.f,0.f,1.f,1.f };

    cam.reflectionBuffer.clear(cro::Colour::Red);
    //don't want to test against skybox depth values.
    m_skyboxScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    m_gameScene.render();
    cam.reflectionBuffer.display();

    cam.setActivePass(cro::Camera::Pass::Final);
    cam.renderFlags = RenderFlags::All;
    cam.viewport = oldVP;

    skyCam.setActivePass(cro::Camera::Pass::Final);
    skyCam.renderFlags = RenderFlags::All;
    skyCam.viewport = oldVP;



    m_gameSceneTexture.clear();
    m_skyboxScene.render();

    glClear(GL_DEPTH_BUFFER_BIT);
    m_gameScene.render();
    m_gameSceneTexture.display();

    m_uiScene.render();
}

//private
void PlaylistState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_skyboxScene.addSystem<cro::CallbackSystem>(mb);
    m_skyboxScene.addSystem<cro::CameraSystem>(mb);
    m_skyboxScene.addSystem<cro::ModelRenderer>(mb);

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimationSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void PlaylistState::loadAssets()
{
    //materials
    m_resources.shaders.loadFromString(ShaderID::Horizon, HorizonVert, HorizonFrag);
    auto* shader = &m_resources.shaders.get(ShaderID::Horizon);
    m_materialIDs[MaterialID::Horizon] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Water, WaterVertex, WaterFragment);
    shader = &m_resources.shaders.get(ShaderID::Water);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Water] = m_resources.materials.add(*shader);


    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Cel);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define NOCHEX\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Course] = m_resources.materials.add(*shader);





    //audio - TODO do we need to keep the audio scape as a member?
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void PlaylistState::buildScene()
{
    //water plane
    auto meshID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(100.f, 30));
    auto waterEnt = m_gameScene.createEntity();
    waterEnt.addComponent<cro::Transform>().setPosition({0.f, WaterLevel, 0.f});
    waterEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::Water]);
    material.setProperty("u_radius", 100.f);
    waterEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_gameScene.setWaterLevel(WaterLevel);
    m_skyboxScene.setWaterLevel(WaterLevel);
    m_skyboxScene.getActiveCamera().getComponent<cro::Camera>().reflectionBuffer.create(2, 2);

    //island
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/cart.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 1.f, 0.f });
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
        for (auto i = 1u; i < entity.getComponent<cro::Model>().getMeshData().submeshCount; ++i)
        {
            applyMaterialData(md, material, i);
            entity.getComponent<cro::Model>().setMaterial(i, material);
        }
    }

    if (md.loadFromFile("assets/golf/models/island.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.f });
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Course]);
        for (auto i = 0u; i < entity.getComponent<cro::Model>().getMeshData().submeshCount; ++i)
        {
            applyMaterialData(md, material, i);
            entity.getComponent<cro::Model>().setMaterial(i, material);
        }

        m_collisionMesh.updateCollisionMesh(entity.getComponent<cro::Model>().getMeshData());
    }



    //distributions
    static constexpr float GrassDensity = 1.7f;
    static constexpr float TreeDensity = 4.f;

    //this matches the image, but the output then needs to be scaled down 5x and offet to centre
    static constexpr std::array MinBounds = { 0.f, 0.f };
    static constexpr std::array MaxBounds = { static_cast<float>(MapSize.x), static_cast<float>(MapSize.y) };

    auto seed = static_cast<std::uint32_t>(std::time(nullptr));
    auto grass = pd::PoissonDiskSampling(GrassDensity, MinBounds, MaxBounds, 30u, seed);
    auto trees = pd::PoissonDiskSampling(TreeDensity, MinBounds, MaxBounds);
    auto flowers = pd::PoissonDiskSampling(TreeDensity * 0.5f, MinBounds, MaxBounds, 30u, seed / 2);


    //3D camera
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float maxScale = std::floor(winSize.y / vpSize.y);
        float scale = m_sharedData.pixelScale ? maxScale : 1.f;
        auto texSize = winSize / scale;
        m_gameSceneTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y));

        auto invScale = (maxScale + 1.f) - scale;
        glCheck(glPointSize(invScale * BallPointSize));
        glCheck(glLineWidth(invScale));

        m_scaleBuffer.setData(invScale);

        m_resolutionUpdate.resolutionData.resolution = texSize / invScale;
        m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);


        //this lets the shader scale leaf billboards correctly
        if (m_sharedData.treeQuality == SharedStateData::High)
        {
            auto targetHeight = texSize.y;
            glUseProgram(m_resources.shaders.get(ShaderID::TreesetLeaf).getGLHandle());
            glUniform1f(m_resources.shaders.get(ShaderID::TreesetLeaf).getUniformID("u_targetHeight"), targetHeight);
        }

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, texSize.x / texSize.y, 0.1f, 115.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setOrigin({0.f, -0.5f, -12.f});
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -15.f * cro::Util::Const::degToRad);

    static constexpr std::uint32_t ReflectionMapSize = 1024u;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    cam.reflectionBuffer.setSmooth(true);
    cam.resizeCallback = updateView;
    updateView(cam);

    auto rootEnt = m_gameScene.createEntity();
    rootEnt.addComponent<cro::Transform>().setPosition({0.f, -1.f, 0.f});
    rootEnt.addComponent<cro::Callback>().active = true;
    rootEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.1f);
    };
    rootEnt.getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());


    //light direction
    auto sunEnt = m_gameScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -130.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -75.f * cro::Util::Const::degToRad);
}

void PlaylistState::buildUI()
{
    //renders main scene
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_gameSceneTexture.getTexture());
    auto sceneEnt = entity;

    //attach UI to this so we have a single root scale
    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>();

    //use a 9-patch to create the tab view background
    auto ninePatch = m_uiScene.createEntity();
    ninePatch.addComponent<cro::Transform>().setPosition({0.f, 0.f, -0.5f});
    ninePatch.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    ninePatch.getComponent<cro::Drawable2D>().setTexture(&m_resources.textures.get("assets/golf/images/tab_patch.png"));
    rootNode.getComponent<cro::Transform>().addChild(ninePatch.getComponent<cro::Transform>());

    auto updateView = [&, sceneEnt, rootNode, ninePatch](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);

        updateNinePatch(ninePatch); //requires scale to be set above

        //resets the sprite size with the updated texture size
        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        sceneEnt.getComponent<cro::Transform>().setScale(courseScale);
        sceneEnt.getComponent<cro::Sprite>().setTexture(m_gameSceneTexture.getTexture());


        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());


    auto textSelected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        });
    auto textUnelected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });


    createSkyboxMenu(rootNode, textSelected, textUnelected);
    createShrubberyMenu(rootNode);
    createHoleMenu(rootNode);
    createFileSystemMenu(rootNode);

    //TODO load sprite sheet and crate tab bar for each menu



}

void PlaylistState::createSkyboxMenu(cro::Entity rootNode, std::uint32_t selected, std::uint32_t unselected)
{
    m_menuEntities[MenuID::Skybox] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Skybox].addComponent<cro::Transform>());

    m_skyboxes = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + SkyboxPath);
    //TODO we want as good a way as possible to validate the files...
    m_skyboxes.erase(std::remove_if(m_skyboxes.begin(), m_skyboxes.end(), 
        [](const std::string& box)
        {
            return cro::FileSystem::getFileExtension(box) != ".sbf";
        }), m_skyboxes.end());
    //just to make consistent across platforms
    std::sort(m_skyboxes.begin(), m_skyboxes.end());

    auto scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    scrollNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    scrollNode.addComponent<UIElement>().absolutePosition = { 8.f, 0.f };
    scrollNode.getComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight * 0.92f };
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());


    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    glm::vec2 position(0.f);
    for (auto i = 0u; i < m_skyboxes.size(); ++i)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(m_skyboxes[i]);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);

        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Skybox);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        const auto& ents = m_skyboxScene.getSystem<cro::ModelRenderer>()->getEntities();
                        for (auto e : ents)
                        {
                            m_skyboxScene.destroyEntity(e);
                        }

                        loadSkybox(SkyboxPath + m_skyboxes[i], m_skyboxScene, m_resources, m_materialIDs[MaterialID::Horizon]);
                        m_skyboxIndex = i;
                        m_courseData.skyboxPath = SkyboxPath + m_skyboxes[i];
                    }                
                });

        position.y -= 10.f;
        scrollNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }


    if (!m_skyboxes.empty())
    {
        m_skyboxIndex = 0;
        auto cloudPath = loadSkybox(SkyboxPath + m_skyboxes[m_skyboxIndex], m_skyboxScene, m_resources, m_materialIDs[MaterialID::Horizon]);
        //TODO load clouds (??)

        m_courseData.skyboxPath = SkyboxPath + m_skyboxes[m_skyboxIndex];
    }
}

void PlaylistState::createShrubberyMenu(cro::Entity rootNode)
{
    m_menuEntities[MenuID::Shrubbery] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Shrubbery].addComponent<cro::Transform>());


    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}

void PlaylistState::createHoleMenu(cro::Entity rootNode)
{
    m_menuEntities[MenuID::Holes] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Holes].addComponent<cro::Transform>());


    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}

void PlaylistState::createFileSystemMenu(cro::Entity rootNode)
{
    m_menuEntities[MenuID::FileSystem] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::FileSystem].addComponent<cro::Transform>());


    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}

void PlaylistState::updateNinePatch(cro::Entity entity)
{
    static constexpr float NinePatchSize = 4.f; //size of a 'patch' in the texture
    
    //0    2
    //|  / |
    //| /  |
    //1    3

    constexpr float PatchU = 1.f / 3.f;
    constexpr float PatchV = 1.f / 3.f;

    constexpr std::array<glm::vec2, 36u> PatchUVs =
    {
        //TL, Bl, TR, BR
        glm::vec2(0.f, 1.f),
        glm::vec2(0.f, PatchV * 2.f),
        glm::vec2(PatchU, 1.f),
        glm::vec2(PatchU, PatchV * 2.f),

        glm::vec2(0.f, PatchV),
        glm::vec2(0.f, 0.f),
        glm::vec2(PatchU, PatchV),
        glm::vec2(PatchU, 0.f),

        glm::vec2(PatchU * 2.f, 1.f),
        glm::vec2(PatchU * 2.f, PatchV * 2.f),
        glm::vec2(1.f, 1.f),
        glm::vec2(1.f, PatchV * 2.f),

        glm::vec2(PatchU * 2.f, PatchV),
        glm::vec2(PatchU * 2.f, 0.f),
        glm::vec2(1.f, PatchV),
        glm::vec2(1.f, 0.f),

        //T, B, L, R
        glm::vec2(PatchU, 1.f),
        glm::vec2(PatchU, PatchV * 2.f),
        glm::vec2(PatchU * 2.f, 1.f),
        glm::vec2(PatchU * 2.f, PatchV * 2.f),

        glm::vec2(PatchU, PatchV),
        glm::vec2(PatchU, 0.f),
        glm::vec2(PatchU * 2.f, PatchV),
        glm::vec2(PatchU * 2.f, 0.f),
        
        glm::vec2(0.f, PatchV * 2.f),
        glm::vec2(0.f, PatchV),
        glm::vec2(PatchU, PatchV * 2.f),
        glm::vec2(PatchU, PatchV),

        glm::vec2(PatchU * 2.f, PatchV * 2.f),
        glm::vec2(PatchU * 2.f, PatchV),
        glm::vec2(1.f, PatchV * 2.f),
        glm::vec2(1.f, PatchV),

        //Centre
        glm::vec2(PatchU, PatchV * 2.f),
        glm::vec2(PatchU, PatchV),
        glm::vec2(PatchU * 2.f, PatchV * 2.f),
        glm::vec2(PatchU * 2.f, PatchV),
    };

    glm::vec2 texSize = entity.getComponent<cro::Drawable2D>().getTexture()->getSize();
    const float PatchWidth = std::floor(texSize.x / 3.f);
    const float PatchHeight = std::floor(texSize.y / 3.f);

    auto size = glm::vec2(cro::App::getWindow().getSize()) / m_viewScale;
    size.x = std::floor(size.x);
    size.y = std::floor(size.y * TabAreaHeight);

    std::array<glm::vec2, 36> positions =
    {
        //TL, Bl, TR, BR
        glm::vec2(0.f, size.y),
        glm::vec2(0.f, size.y - PatchHeight),
        glm::vec2(PatchWidth, size.y),
        glm::vec2(PatchWidth, size.y - PatchHeight),

        glm::vec2(0.f, PatchHeight),
        glm::vec2(0.f, 0.f),
        glm::vec2(PatchWidth, PatchHeight),
        glm::vec2(PatchWidth, 0.f),

        glm::vec2(size.x - PatchWidth, size.y),
        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x, size.y),
        glm::vec2(size.x, size.y - PatchHeight),

        glm::vec2(size.x - PatchWidth, PatchHeight),
        glm::vec2(size.x - PatchWidth, 0.f),
        glm::vec2(size.x, PatchHeight),
        glm::vec2(size.x, 0.f),

        //T, B, L, R
        glm::vec2(PatchWidth, size.y),
        glm::vec2(PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x - PatchWidth, size.y),
        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),

        glm::vec2(PatchWidth, PatchHeight),
        glm::vec2(PatchWidth, 0.f),
        glm::vec2(size.x - PatchWidth, PatchHeight),
        glm::vec2(size.x - PatchWidth, 0.f),

        glm::vec2(0.f, size.y - PatchHeight),
        glm::vec2(0.f, PatchHeight),
        glm::vec2(PatchWidth, size.y - PatchHeight),
        glm::vec2(PatchWidth, PatchHeight),

        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x - PatchWidth, PatchHeight),
        glm::vec2(size.x, size.y - PatchHeight),
        glm::vec2(size.x, PatchHeight),

        //Center
        glm::vec2(PatchWidth, size.y - PatchHeight),
        glm::vec2(PatchWidth, PatchHeight),
        glm::vec2(size.x - PatchWidth, size.y - PatchHeight),
        glm::vec2(size.x - PatchWidth, PatchHeight),
    };

    //would be so much smarter to index the verts instead of duping them
    //but we're in it up to our beards now.
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(positions[0], PatchUVs[0]),
            cro::Vertex2D(positions[1], PatchUVs[1]),
            cro::Vertex2D(positions[2], PatchUVs[2]),
            cro::Vertex2D(positions[1], PatchUVs[1]),
            cro::Vertex2D(positions[3], PatchUVs[3]),
            cro::Vertex2D(positions[2], PatchUVs[2]),


            cro::Vertex2D(positions[4], PatchUVs[4]),
            cro::Vertex2D(positions[5], PatchUVs[5]),
            cro::Vertex2D(positions[6], PatchUVs[6]),
            cro::Vertex2D(positions[5], PatchUVs[5]),
            cro::Vertex2D(positions[7], PatchUVs[7]),
            cro::Vertex2D(positions[6], PatchUVs[6]),


            cro::Vertex2D(positions[8],  PatchUVs[8]),
            cro::Vertex2D(positions[9],  PatchUVs[9]),
            cro::Vertex2D(positions[10], PatchUVs[10]),
            cro::Vertex2D(positions[9],  PatchUVs[9]),
            cro::Vertex2D(positions[11], PatchUVs[11]),
            cro::Vertex2D(positions[10], PatchUVs[10]),


            cro::Vertex2D(positions[12], PatchUVs[12]),
            cro::Vertex2D(positions[13], PatchUVs[13]),
            cro::Vertex2D(positions[14], PatchUVs[14]),
            cro::Vertex2D(positions[13], PatchUVs[13]),
            cro::Vertex2D(positions[15], PatchUVs[15]),
            cro::Vertex2D(positions[14], PatchUVs[14]),


            cro::Vertex2D(positions[16], PatchUVs[16]),
            cro::Vertex2D(positions[17], PatchUVs[17]),
            cro::Vertex2D(positions[18], PatchUVs[18]),
            cro::Vertex2D(positions[17], PatchUVs[17]),
            cro::Vertex2D(positions[19], PatchUVs[19]),
            cro::Vertex2D(positions[18], PatchUVs[18]),


            cro::Vertex2D(positions[20], PatchUVs[20]),
            cro::Vertex2D(positions[21], PatchUVs[21]),
            cro::Vertex2D(positions[22], PatchUVs[22]),
            cro::Vertex2D(positions[21], PatchUVs[21]),
            cro::Vertex2D(positions[23], PatchUVs[23]),
            cro::Vertex2D(positions[22], PatchUVs[22]),


            cro::Vertex2D(positions[24], PatchUVs[24]),
            cro::Vertex2D(positions[25], PatchUVs[25]),
            cro::Vertex2D(positions[26], PatchUVs[26]),
            cro::Vertex2D(positions[25], PatchUVs[25]),
            cro::Vertex2D(positions[27], PatchUVs[27]),
            cro::Vertex2D(positions[26], PatchUVs[26]),


            cro::Vertex2D(positions[28], PatchUVs[28]),
            cro::Vertex2D(positions[29], PatchUVs[29]),
            cro::Vertex2D(positions[30], PatchUVs[30]),
            cro::Vertex2D(positions[29], PatchUVs[29]),
            cro::Vertex2D(positions[31], PatchUVs[31]),
            cro::Vertex2D(positions[30], PatchUVs[30]),


            cro::Vertex2D(positions[32], PatchUVs[32]),
            cro::Vertex2D(positions[33], PatchUVs[33]),
            cro::Vertex2D(positions[34], PatchUVs[34]),
            cro::Vertex2D(positions[33], PatchUVs[33]),
            cro::Vertex2D(positions[35], PatchUVs[35]),
            cro::Vertex2D(positions[34], PatchUVs[34])
        });
}

void PlaylistState::quitState()
{
    //TODO some sort of confirmation, make sure everything is saved
    requestStackClear();
    requestStackPush(StateID::Menu);
}