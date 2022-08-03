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
#include "Billboard.hpp"
#include "Treeset.hpp"
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
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include "../ErrorCheck.hpp"

namespace
{
#include "WaterShader.inl"
#include "TerrainShader.inl"
#include "BillboardShader.inl"
#include "TreeShader.inl"
#include "ShadowMapping.inl"

    const std::string SkyboxPath = "assets/golf/skyboxes/";
    const std::string ShrubPath = "assets/golf/shrubs/";
    const std::string CoursePath = "assets/golf/courses/";
    const std::string ThumbPath = "assets/golf/thumbs/";

    constexpr float TabAreaHeight = 0.25f; //percent of screen
    constexpr float ItemSpacing = 10.f;
    
    struct ScrollData final
    {
        explicit ScrollData(std::size_t ic) : itemCount(ic) {}
        const std::size_t itemCount = 0;

        std::size_t currIndex = 0;
        std::size_t targetIndex = 0;
        glm::vec3 basePosition = glm::vec3(0.f);
    };

    struct ScrollNodeCallback final
    {
        void operator ()(cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<ScrollData>();

            auto targetPos = data.basePosition;
            targetPos.y += ItemSpacing * data.targetIndex;

            auto movement = targetPos - e.getComponent<cro::Transform>().getPosition();
            if (glm::length2(movement) > 1)
            {
                e.getComponent<cro::Transform>().move(movement * (dt * 10.f));
            }
            else
            {
                e.getComponent<cro::Transform>().setPosition(targetPos);
                data.currIndex = data.targetIndex;
            }
        };
    };

    struct ListItemCallback final
    {
        const cro::FloatRect& croppingArea;
        ListItemCallback(const cro::FloatRect& c)
            : croppingArea(c) {}

        void operator ()(cro::Entity e, float)
        {
            auto cropRect = croppingArea;
            auto localBounds = e.getComponent<cro::Drawable2D>().getLocalBounds();
            auto pos = e.getComponent<cro::Transform>().getWorldPosition();
            cropRect.left -= pos.x;
            cropRect.bottom -= pos.y;
            cropRect.bottom -= localBounds.bottom + localBounds.height;
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropRect);

            float scale = 1.f;
            pos.y -= (localBounds.height / 2.f);
            if (!croppingArea.contains(pos))
            {
                scale = 0.f;
            }
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        }
    };

    cro::Entity createHighlight(cro::Scene& scene, const cro::SpriteSheet& spriteSheet)
    {
        auto entity = scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("scroll_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f), -0.3f });

        entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();

        return entity;
    }
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
    m_skyboxIndex       (0),
    m_shrubIndex        (0),
    m_currentTab        (0)
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

#ifdef CRO_DEBUG_
    /*registerWindow([&]()
        {
            if (ImGui::Begin("buns"))
            {
                
            }
            ImGui::End();
        });*/
#endif
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
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            quitState();
            return false;
        case SDLK_1:
            setActiveTab(0);
            break;
        case SDLK_2:
            setActiveTab(1);
            break;
        case SDLK_3:
            setActiveTab(2);
            break;
        case SDLK_4:
            setActiveTab(3);
            break;
        case SDLK_p:
        case SDLK_PAUSE:
        case SDLK_ESCAPE:
            requestStackPush(StateID::Pause);
            break;
#ifdef CRO_DEBUG_
        case SDLK_F9:
            requestStackPush(StateID::Bush);
            break;
#endif
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        switch (evt.cbutton.button)
        {
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonBack:
            quitState();
            return false;
        case cro::GameController::ButtonLeftShoulder:
            m_currentTab = (m_currentTab + (MenuID::Count - 2)) % (MenuID::Count - 1);
            setActiveTab(m_currentTab);
            break;
        case cro::GameController::ButtonRightShoulder:
            m_currentTab = (m_currentTab + 1) % (MenuID::Count - 1);
            setActiveTab(m_currentTab);
            break;
        case cro::GameController::ButtonStart:
            requestStackPush(StateID::Pause);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        /*if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }*/
    }

    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            cro::ButtonEvent fakeEvent;
            fakeEvent.type = SDL_MOUSEBUTTONDOWN;
            fakeEvent.button.button = SDL_BUTTON_LEFT;

            //up
            auto menuID = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
            if (menuID == MenuID::Skybox)
            {
                m_callbacks[CallbackID::SkyScrollUp](cro::Entity(), fakeEvent);
            }
            else if (menuID == MenuID::Shrubbery)
            {
                m_callbacks[CallbackID::ShrubScrollUp](cro::Entity(), fakeEvent);
            }
        }
        else if (evt.wheel.y < 0)
        {
            cro::ButtonEvent fakeEvent;
            fakeEvent.type = SDL_MOUSEBUTTONDOWN;
            fakeEvent.button.button = SDL_BUTTON_LEFT;

            //down
            auto menuID = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
            if (menuID == MenuID::Skybox)
            {
                m_callbacks[CallbackID::SkyScrollDown](cro::Entity(), fakeEvent);
            }
            else if (menuID == MenuID::Shrubbery)
            {
                m_callbacks[CallbackID::ShrubScrollDown](cro::Entity(), fakeEvent);
            }
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
    windData.direction[0] = 1.f;
    windData.direction[1] = 0.6f;

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

    glEnable(GL_PROGRAM_POINT_SIZE);

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
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb)->setMaxDistance(50.f);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
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

    m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader);
    shader = &m_resources.shaders.get(ShaderID::Billboard);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Billboard] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::TreesetBranch, BranchVertex, BranchFragment, "#define INSTANCING\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetBranch);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Branch] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::TreesetLeaf, BushVertex, BushFragment, "#define INSTANCING\n#define HQ\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetLeaf);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Leaf] = m_resources.materials.add(*shader);
    m_sharedData.treeQuality;

    m_resources.shaders.loadFromString(ShaderID::TreesetShadow, ShadowVertex, ShadowFragment, "#define INSTANCING\n#define TREE_WARP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetShadow);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::BranchShadow] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::TreesetLeafShadow, ShadowVertex, ShadowFragment, "#define INSTANCING\n#define LEAF_SIZE\n#define ALPHA_CLIP\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::TreesetLeafShadow);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::LeafShadow] = m_resources.materials.add(*shader);
    m_sharedData.treeQuality;


    //audio - TODO do we need to keep the audio scape as a member?
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");
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

    static constexpr std::array MinBounds = { 0.f, 0.f };
    static constexpr std::array MaxBounds = { static_cast<float>(MapSize.x) / 5.f, static_cast<float>(MapSize.y) / 5.f };

    auto seed = static_cast<std::uint32_t>(std::time(nullptr));
    auto grass = pd::PoissonDiskSampling(GrassDensity, MinBounds, MaxBounds, 30u, seed);
    auto trees = pd::PoissonDiskSampling(TreeDensity, MinBounds, MaxBounds);
    auto flowers = pd::PoissonDiskSampling(TreeDensity * 0.5f, MinBounds, MaxBounds, 30u, seed / 2);

    auto distributionToWorldPoints = 
        [&](const std::vector<std::array<float, 2u>>& dist, std::vector<glm::vec3>& output, std::int32_t terrainID)
    {
        constexpr glm::vec3 offset(-32.f, 0.f, 20.f);
        for (const auto& p : dist)
        {
            glm::vec3 position(p[0], 0.f, -p[1]);
            position += offset;
            auto result = m_collisionMesh.getTerrain(position);
            if (result.terrain == terrainID
                && result.height > WaterLevel)
            {
                position.y = result.height - 0.05f;
                output.push_back(position);
            }
        }
    };
    distributionToWorldPoints(grass, m_grassDistribution, TerrainID::Rough);
    distributionToWorldPoints(trees, m_treeDistribution, TerrainID::Scrub);
    distributionToWorldPoints(flowers, m_flowerDistribution, TerrainID::Scrub);

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
        auto targetHeight = texSize.y;
        glUseProgram(m_resources.shaders.get(ShaderID::TreesetLeaf).getGLHandle());
        glUniform1f(m_resources.shaders.get(ShaderID::TreesetLeaf).getUniformID("u_targetHeight"), targetHeight);

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, texSize.x / texSize.y, 0.1f, 115.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({0.f, 2.5f, 15.f});
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -15.f * cro::Util::Const::degToRad);

    static constexpr std::uint32_t ReflectionMapSize = 1024u;
    static constexpr std::uint32_t ShadowMapSize = 2048u;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    cam.reflectionBuffer.setSmooth(true);
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    cam.resizeCallback = updateView;
    updateView(cam);

    auto rootEnt = m_gameScene.createEntity();
    rootEnt.addComponent<cro::Transform>().setPosition({ 0.f, 1.f, 0.f });
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
    auto textUnselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });


    auto highlightSelected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        });
    auto highlightUnselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/course_tabs.spt", m_resources.textures);

    MenuData md;
    md.textSelected = textSelected;
    md.textUnselected = textUnselected;
    md.scrollSelected = highlightSelected;
    md.scrollUnselected = highlightUnselected;
    md.spriteSheet = &spriteSheet;

    createSkyboxMenu(rootNode, md);
    createShrubberyMenu(rootNode, md);
    createHoleMenu(rootNode);
    createFileSystemMenu(rootNode);


    m_animationIDs[AnimationID::TabHoles] = spriteSheet.getAnimationIndex("holes", "tab_bar");
    m_animationIDs[AnimationID::TabSkybox] = spriteSheet.getAnimationIndex("skybox", "tab_bar");
    m_animationIDs[AnimationID::TabSaveLoad] = spriteSheet.getAnimationIndex("save_load", "tab_bar");
    m_animationIDs[AnimationID::TabShrubs] = spriteSheet.getAnimationIndex("shrubs", "tab_bar");


    //tab bar
    auto tabEnt = m_uiScene.createEntity();
    tabEnt.addComponent<cro::Transform>();
    tabEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    tabEnt.addComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };
    tabEnt.addComponent<cro::Drawable2D>();
    tabEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_bar");
    tabEnt.addComponent<cro::SpriteAnimation>().play(m_animationIDs[AnimationID::TabSkybox]);
    rootNode.getComponent<cro::Transform>().addChild(tabEnt.getComponent<cro::Transform>());

    //we should never attempt to acces a menu at this index so we'll use it as a hacky convenience
    //to store the entity with the tab graphics (setActiveTab() will want to update the animation)
    m_tabEntities[MenuID::Dummy] = tabEnt;

    auto tabSelect = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                setActiveTab(e.getComponent<cro::Callback>().getUserData<std::int32_t>());
            }
        });

    constexpr glm::vec2 buttonOffset(15.f, 1.f);
    auto bounds = spriteSheet.getSprite("tab_highlight").getTextureBounds();
    auto tabWidth = spriteSheet.getSprite("tab_bar").getTextureBounds().width / 4.f;

    for (auto i = 0u; i < MenuID::Count -1; ++i)
    {
        glm::vec3 position((i * tabWidth) + buttonOffset.x, buttonOffset.y, 0.1f);
        position.x += bounds.width / 2.f;
        position.y += bounds.height / 2.f;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
        entity.getComponent<cro::Callback>().setUserData<std::int32_t>(i); //use this with button activated callback to know which index we we are

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = highlightSelected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = highlightUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = tabSelect;

        tabEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_tabEntities[i] = entity;
    }

    //needs a one frame delay before first call
    //to allow entities to be added to the ui system
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //setActiveTab(MenuID::Shrubbery);
        setActiveTab(MenuID::Skybox);
        e.getComponent<cro::Callback>().active = false;
        m_uiScene.destroyEntity(e);
    };


    //displays info about current selections
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 4.f, -12.f };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_infoEntity = entity;
    updateInfo();


    //TODO when we know what size this is make some nicer artwork
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -2.f, 4.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(0.f, -80.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(150.f, 0.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(150.f, -80.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha))
        });
    m_infoEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void PlaylistState::createSkyboxMenu(cro::Entity rootNode, const MenuData& menuData)
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

    if (m_skyboxes.empty())
    {
        m_skyboxes.push_back("No skybox files found");
        return;
    }

    m_menuEntities[MenuID::Skybox].addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    m_menuEntities[MenuID::Skybox].addComponent<UIElement>().absolutePosition = { 8.f, -8.f };
    m_menuEntities[MenuID::Skybox].getComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };
    
    auto scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());

    //scroll callbacks
    ScrollData scrollData(m_skyboxes.size());

    scrollData.basePosition = scrollNode.getComponent<cro::Transform>().getPosition();

    scrollNode.addComponent<cro::Callback>().setUserData<ScrollData>(scrollData);
    scrollNode.getComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function = ScrollNodeCallback();

    m_callbacks[CallbackID::SkyScrollDown] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            data.targetIndex = std::min(data.targetIndex + 1, data.itemCount - 1);
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_callbacks[CallbackID::SkyScrollUp] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (data.currIndex > 0)
            {
                data.targetIndex = data.currIndex - 1;
            }
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        }
    };

    //scroll buttons
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_up");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, -std::floor(bounds.height * 1.25f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    auto buttonEnt = entity;
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f, });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Skybox);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::SkyScrollUp]);


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_down");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, std::floor(bounds.height * 2.f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, -TabAreaHeight };
    buttonEnt = entity;
    m_menuEntities[MenuID::Skybox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f, });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Skybox);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::SkyScrollDown]);


    //list of skybox items
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

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);

        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Skybox);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = 
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&,i, scrollNode](cro::Entity e) mutable
                {
                    e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                    auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                    pos.y -= ItemSpacing / 2.f;
                    if (!m_croppingArea.contains(pos))
                    {
                        cro::ButtonEvent fakeEvent;
                        fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                        fakeEvent.button.button = SDL_BUTTON_LEFT;

                        if (pos.y < m_croppingArea.bottom)
                        {
                            m_callbacks[CallbackID::SkyScrollDown](cro::Entity(), fakeEvent);
                        }
                        else if (pos.y > (m_croppingArea.bottom + m_croppingArea.height))
                        {
                            scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = i;
                        }
                    }
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
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

                        updateInfo();
                    }
                });

        position.y -= ItemSpacing;
        scrollNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //load default skybox if found
    if (!m_skyboxes.empty())
    {
        m_skyboxIndex = 0;
        auto cloudPath = loadSkybox(SkyboxPath + m_skyboxes[m_skyboxIndex], m_skyboxScene, m_resources, m_materialIDs[MaterialID::Horizon]);
        //TODO load clouds (??)

        m_courseData.skyboxPath = SkyboxPath + m_skyboxes[m_skyboxIndex];
    }
}

void PlaylistState::createShrubberyMenu(cro::Entity rootNode, const MenuData& menuData)
{
    m_menuEntities[MenuID::Shrubbery] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Shrubbery].addComponent<cro::Transform>());

    m_shrubs = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + ShrubPath);
    m_shrubs.erase(std::remove_if(m_shrubs.begin(), m_shrubs.end(),
        [](const std::string& box)
        {
            return cro::FileSystem::getFileExtension(box) != ".shb";
        }), m_shrubs.end());
    //just to make consistent across platforms
    std::sort(m_shrubs.begin(), m_shrubs.end());

    if (m_shrubs.empty())
    {
        m_shrubs.push_back("No shrub files found");
        return;
    }

    static constexpr float RootOffset = 8.f;
    m_menuEntities[MenuID::Shrubbery].addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    m_menuEntities[MenuID::Shrubbery].addComponent<UIElement>().absolutePosition = { RootOffset, -8.f };
    m_menuEntities[MenuID::Shrubbery].getComponent<UIElement>().relativePosition = { 0.f, TabAreaHeight };
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().setScale(glm::vec2(0.f));


    auto scrollNode = m_uiScene.createEntity();
    scrollNode.addComponent<cro::Transform>();
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());

    //scroll callbacks
    ScrollData scrollData(m_shrubs.size());

    scrollData.basePosition = scrollNode.getComponent<cro::Transform>().getPosition();

    scrollNode.addComponent<cro::Callback>().setUserData<ScrollData>(scrollData);
    scrollNode.getComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function = ScrollNodeCallback();

    m_callbacks[CallbackID::ShrubScrollDown] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            data.targetIndex = std::min(data.targetIndex + 1, data.itemCount - 1);
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        }
    };
    m_callbacks[CallbackID::ShrubScrollUp] =
        [&, scrollNode](cro::Entity, const cro::ButtonEvent& evt) mutable
    {
        if (activated(evt))
        {
            auto& data = scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (data.currIndex > 0)
            {
                data.targetIndex = data.currIndex - 1;
            }
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
        }
    };

    //scroll buttons
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_up");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, -std::floor(bounds.height * 1.25f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    auto buttonEnt = entity;
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::ShrubScrollUp]);


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_down");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().absolutePosition = { -bounds.width * 3.f, std::floor(bounds.height * 2.f) };
    entity.getComponent<UIElement>().relativePosition = { 1.f, -TabAreaHeight };
    buttonEnt = entity;
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(m_callbacks[CallbackID::ShrubScrollDown]);

    //list of shrub items
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    glm::vec2 position(0.f);
    for (auto i = 0u; i < m_shrubs.size(); ++i)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(m_shrubs[i]);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = ListItemCallback(m_croppingArea);

        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i, scrollNode](cro::Entity e) mutable
                {
                    e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                    auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                    pos.y -= ItemSpacing / 2.f;
                    if (!m_croppingArea.contains(pos))
                    {
                        cro::ButtonEvent fakeEvent;
                        fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                        fakeEvent.button.button = SDL_BUTTON_LEFT;

                        if (pos.y < m_croppingArea.bottom)
                        {
                            m_callbacks[CallbackID::ShrubScrollDown](cro::Entity(), fakeEvent);
                        }
                        else if (pos.y > (m_croppingArea.bottom + m_croppingArea.height))
                        {
                            scrollNode.getComponent<cro::Callback>().getUserData<ScrollData>().targetIndex = i;
                        }
                    }
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.textUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt)
                        && m_shrubIndex != i)
                    {
                        for (auto& shrub : m_shrubberyModels)
                        {
                            shrub.hide();
                        }

                        m_shrubIndex = i;
                        m_courseData.shrubPath = ShrubPath + m_shrubs[i];
                        applyShrubQuality();
                        updateInfo();
                    }
                });

        position.y -= ItemSpacing;
        scrollNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        loadShrubbery(ShrubPath + m_shrubs[i]);
        m_shrubberyModels[i].hide();
    }

    //load default shrub if found
    if (!m_shrubs.empty())
    {
        m_shrubIndex = 0;
        m_courseData.shrubPath = ShrubPath + m_shrubs[m_shrubIndex];

        applyShrubQuality();
    }

    //buttons for toggling quality setting
    static const std::array<std::string, 3u> Labels =
    {
        "Classic", "Low", "High"
    };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Tree Quality");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 38.f };
    centreText(entity);
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(Labels[m_sharedData.treeQuality]);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -RootOffset, 24.f };
    centreText(entity);
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto labelEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_left");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { -30.f - RootOffset, 16.f };
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto scrollEnt = entity;

    bounds = menuData.spriteSheet->getSprite("scroll_highlight").getTextureBounds();
    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, labelEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.treeQuality = (m_sharedData.treeQuality + 2) % 3;
                    labelEnt.getComponent<cro::Text>().setString(Labels[m_sharedData.treeQuality]);
                    centreText(labelEnt);
                    applyShrubQuality();
                }
            });
    scrollEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = menuData.spriteSheet->getSprite("scroll_right");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().relativePosition = { 0.5f, -TabAreaHeight };
    entity.getComponent<UIElement>().absolutePosition = { 21.f - RootOffset, 16.f };
    m_menuEntities[MenuID::Shrubbery].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    scrollEnt = entity;

    entity = createHighlight(m_uiScene, *menuData.spriteSheet);
    entity.getComponent<cro::Transform>().setPosition({ 4.f, 4.f });
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Shrubbery);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = menuData.scrollSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = menuData.scrollUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, labelEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.treeQuality = (m_sharedData.treeQuality + 1) % 3;
                    labelEnt.getComponent<cro::Text>().setString(Labels[m_sharedData.treeQuality]);
                    centreText(labelEnt);
                    applyShrubQuality();
                }
            });
    scrollEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void PlaylistState::createHoleMenu(cro::Entity rootNode)
{
    m_menuEntities[MenuID::Holes] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::Holes].addComponent<cro::Transform>());

    //fetch list of directories in course path
    //for each dir fetch files
    //for each file
    //if file is hole file look for thumb with matching name/path.
    //if thumb exists add dir to dir vector, and hole to dirIdx/hole vector
    //create node for each dir to which thumbs are attached
    //show/hide node based on selected dir
    //clicking on thumb adds dir/thumb index to playlist
    //NOTE thumb indices may not necessarily be sequential - it has to match
    //the hole file index, so missing thumbs will jump an index.

    //TODO delete this
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Holes);

    m_menuEntities[MenuID::Holes].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}

void PlaylistState::createFileSystemMenu(cro::Entity rootNode)
{
    m_menuEntities[MenuID::FileSystem] = m_uiScene.createEntity();
    rootNode.getComponent<cro::Transform>().addChild(m_menuEntities[MenuID::FileSystem].addComponent<cro::Transform>());

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::FileSystem);

    m_menuEntities[MenuID::FileSystem].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}

void PlaylistState::setActiveTab(std::int32_t index)
{
    index %= MenuID::Count -1;
    m_currentTab = index;

    for (auto i = 0; i < MenuID::Count - 1; ++i)
    {
        auto tabIndex = m_tabEntities[i].getComponent<cro::Callback>().getUserData<std::int32_t>();
        if (tabIndex == index)
        {
            m_tabEntities[i].getComponent<cro::UIInput>().setGroup(MenuID::Dummy);
            m_tabEntities[i].getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            m_menuEntities[i].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        else
        {
            m_tabEntities[i].getComponent<cro::UIInput>().setGroup(index);
            m_menuEntities[i].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(index);

    //we've stashed the tab graphic entity in here.
    m_tabEntities[MenuID::Dummy].getComponent<cro::SpriteAnimation>().play(m_animationIDs[index]);
}

void PlaylistState::loadShrubbery(const std::string& path)
{
    auto& shrubbery = m_shrubberyModels.emplace_back();
    
    //parse shrub file and look for valid paths
    cro::ConfigFile shrubFile;
    if (shrubFile.loadFromFile(path))
    {
        std::string billboardModel;
        std::string billboardSprite;
        std::vector<std::string> treesetPaths;
        constexpr std::size_t MaxTreesets = 4;

        const auto& props = shrubFile.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                billboardModel = p.getValue<std::string>();
            }
            else if (name == "sprite")
            {
                billboardSprite = p.getValue<std::string>();
            }
            else if (name == "treeset")
            {
                if (treesetPaths.size() < MaxTreesets)
                {
                    treesetPaths.push_back(p.getValue<std::string>());
                }
            }
        }

        //TODO do we want to bail if some info is missing? or just load what we can?
        auto setBillboards = [&](const std::string& modelPath, const std::string& spritePath, std::size_t outIndex)
        {
            cro::SpriteSheet spriteSheet;
            if (spriteSheet.loadFromFile(spritePath, m_resources.textures))
            {
                std::array<cro::Billboard, BillboardID::Count> billboardTemplates = {};
                billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
                billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
                billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
                billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
                billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
                billboardTemplates[BillboardID::Bush01] = spriteToBillboard(spriteSheet.getSprite("hedge01"));
                billboardTemplates[BillboardID::Bush02] = spriteToBillboard(spriteSheet.getSprite("hedge02"));

                billboardTemplates[BillboardID::Tree01] = spriteToBillboard(spriteSheet.getSprite("tree01"));
                billboardTemplates[BillboardID::Tree02] = spriteToBillboard(spriteSheet.getSprite("tree02"));
                billboardTemplates[BillboardID::Tree03] = spriteToBillboard(spriteSheet.getSprite("tree03"));
                billboardTemplates[BillboardID::Tree04] = spriteToBillboard(spriteSheet.getSprite("tree04"));

                cro::ModelDefinition md(m_resources);
                if (md.loadFromFile(modelPath))
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);

                    auto billboardMat = m_resources.materials.get(m_materialIDs[MaterialID::Billboard]);
                    applyMaterialData(md, billboardMat);
                    entity.getComponent<cro::Model>().setMaterial(0, billboardMat);

                    if (entity.hasComponent<cro::BillboardCollection>())
                    {
                        auto& collection = entity.getComponent<cro::BillboardCollection>();

                        for (auto pos : m_grassDistribution)
                        {
                            float scale = static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f;

                            auto bb = billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Grass02)];
                            bb.position = pos;
                            bb.size *= scale;
                            collection.addBillboard(bb);
                        }

                        for (auto pos : m_flowerDistribution)
                        {
                            float scale = static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f;

                            auto bb = billboardTemplates[cro::Util::Random::value(BillboardID::Flowers01, BillboardID::Bush02)];
                            bb.position = pos;
                            bb.size *= scale;
                            collection.addBillboard(bb);
                        }
                    }
                    shrubbery.billboardEnts[outIndex] = entity;

                    //trees are separate as we might want treesets instead
                    md.loadFromFile(modelPath); //reload to create unique VBO
                    entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);
                    entity.getComponent<cro::Model>().setMaterial(0, billboardMat);

                    if (entity.hasComponent<cro::BillboardCollection>())
                    {
                        auto& collection = entity.getComponent<cro::BillboardCollection>();

                        for (auto i = 0u; i < m_treeDistribution.size(); ++i)
                        {
                            auto pos = m_treeDistribution[i];
                            float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;

                            auto idx = BillboardID::Tree01 + (i % ((BillboardID::Tree04 + 1) - BillboardID::Tree01));
                            auto bb = billboardTemplates[idx];
                            bb.position = pos;
                            bb.size *= scale;
                            collection.addBillboard(bb);
                        }
                    }
                    shrubbery.treeBillboardEnts[outIndex] = entity;
                }
            }
        };

        setBillboards(billboardModel, billboardSprite, 0);

        //check if a low quality version is available and load that, else duplicate the
        //current billboard ents into the low Q slot.
        auto billboardModelLow = billboardModel.substr(0, billboardModel.find_last_of('.')) + "_low.cmt";
        auto billboardSpriteLow = billboardSprite.substr(0, billboardSprite.find_last_of('.')) + "_low.spt";

        if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + billboardModelLow)
            && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + billboardSpriteLow))
        {
            setBillboards(billboardModelLow, billboardSpriteLow, 1);
        }
        else
        {
            shrubbery.billboardEnts[1] = shrubbery.billboardEnts[0];
            shrubbery.treeBillboardEnts[1] = shrubbery.treeBillboardEnts[0];
        }

        //load tree sets
        std::vector<Treeset> treesets;
        std::vector<std::vector<glm::mat4>> transforms;
        for (const auto& ts : treesetPaths)
        {
            Treeset treeset;
            if (treeset.loadFromFile(ts))
            {
                transforms.emplace_back();
                treesets.push_back(treeset);
            }
        }

        cro::ModelDefinition md(m_resources);
        for (auto i = 0u; i < MaxTreesets; ++i)
        {
            if (i < treesets.size())
            {
                //load treeset
                if (md.loadFromFile(treesets[i].modelPath), true)
                {
                    shrubbery.treesetEnts[i] = m_gameScene.createEntity();
                    shrubbery.treesetEnts[i].addComponent<cro::Transform>();
                    md.createModel(shrubbery.treesetEnts[i]);

                    for (auto idx : treesets[i].branchIndices)
                    {
                        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Branch]);
                        applyMaterialData(md, material, idx);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setMaterial(idx, material);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setShadowMaterial(idx, m_resources.materials.get(m_materialIDs[MaterialID::BranchShadow]));
                    }

                    auto& meshData = shrubbery.treesetEnts[i].getComponent<cro::Model>().getMeshData();
                    for (auto idx : treesets[i].leafIndices)
                    {
                        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Leaf]);
                        meshData.indexData[idx].primitiveType = GL_POINTS;
                        material.setProperty("u_diffuseMap", m_resources.textures.get(treesets[i].texturePath));
                        material.setProperty("u_leafSize", treesets[i].leafSize);
                        material.setProperty("u_randAmount", treesets[i].randomness);
                        material.setProperty("u_colour", treesets[i].colour);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setMaterial(idx, material);

                        material = m_resources.materials.get(m_materialIDs[MaterialID::LeafShadow]);
                        material.setProperty("u_diffuseMap", m_resources.textures.get(treesets[i].texturePath));
                        material.setProperty("u_leafSize", treesets[i].leafSize);
                        shrubbery.treesetEnts[i].getComponent<cro::Model>().setShadowMaterial(idx, material);
                    }

                    transforms.emplace_back();
                }
            }
            else
            {
                //create billboard in place
                if (md.loadFromFile(billboardModel))
                {
                    shrubbery.treesetEnts[i] = m_gameScene.createEntity();
                    shrubbery.treesetEnts[i].addComponent<cro::Transform>();
                    md.createModel(shrubbery.treesetEnts[i]);
                }
            }
        }

        cro::SpriteSheet spriteSheet;
        spriteSheet.loadFromFile(billboardSprite, m_resources.textures);

        for (auto i = 0u; i < m_treeDistribution.size(); ++i)
        {
            auto idx = i % MaxTreesets;
            if (idx < treesets.size())
            {
                //add to instance positions
                auto tx = glm::translate(glm::mat4(1.f), m_treeDistribution[i]);

                float scale = static_cast<float>(cro::Util::Random::value(16, 20)) / 10.f;
                tx = glm::scale(tx, glm::vec3(scale));

                transforms[idx].push_back(tx);
            }
            else
            {
                const std::array<std::string, 4u> TreeNames =
                {
                    "tree01",
                    "tree02",
                    "tree03",
                    "tree04",
                };

                //add a billboard
                auto bb = spriteToBillboard(spriteSheet.getSprite(TreeNames[idx]));
                bb.size *= static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;
                bb.position = m_treeDistribution[i];

                if (shrubbery.treesetEnts[idx].hasComponent<cro::BillboardCollection>())
                {
                    shrubbery.treesetEnts[idx].getComponent<cro::BillboardCollection>().addBillboard(bb);
                }
            }
        }

        for (auto i = 0u; i < treesets.size(); ++i)
        {
            shrubbery.treesetEnts[i].getComponent<cro::Model>().setInstanceTransforms(transforms[i]);
        }
    }

    //show/hide appropriate ent based on tree quality setting
    applyShrubQuality();
}

void PlaylistState::applyShrubQuality()
{
    auto& shrubbery = m_shrubberyModels[m_shrubIndex];

    switch (m_sharedData.treeQuality)
    {
    default: break;
    case SharedStateData::Classic:
        if (shrubbery.billboardEnts[0].isValid())
        {
            shrubbery.billboardEnts[0].getComponent<cro::Model>().setHidden(true);
            shrubbery.billboardEnts[1].getComponent<cro::Model>().setHidden(false);
        }

        if (shrubbery.treeBillboardEnts[0].isValid())
        {
            shrubbery.treeBillboardEnts[0].getComponent<cro::Model>().setHidden(true);
            shrubbery.treeBillboardEnts[1].getComponent<cro::Model>().setHidden(false);
        }

        for (auto e : shrubbery.treesetEnts)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Model>().setHidden(true);
            }
        }
        break;
    case SharedStateData::Low:
        if (shrubbery.billboardEnts[0].isValid())
        {
            shrubbery.billboardEnts[0].getComponent<cro::Model>().setHidden(false);
            shrubbery.billboardEnts[1].getComponent<cro::Model>().setHidden(shrubbery.billboardEnts[1] != shrubbery.billboardEnts[0]);
        }

        if (shrubbery.treeBillboardEnts[0].isValid())
        {
            shrubbery.treeBillboardEnts[0].getComponent<cro::Model>().setHidden(false);
            shrubbery.treeBillboardEnts[1].getComponent<cro::Model>().setHidden(shrubbery.treeBillboardEnts[1] != shrubbery.treeBillboardEnts[0]);
        }

        for (auto e : shrubbery.treesetEnts)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Model>().setHidden(true);
            }
        }
        break;
    case SharedStateData::High:
        if (shrubbery.billboardEnts[0].isValid())
        {
            shrubbery.billboardEnts[0].getComponent<cro::Model>().setHidden(false);
            shrubbery.billboardEnts[1].getComponent<cro::Model>().setHidden(shrubbery.billboardEnts[1] != shrubbery.billboardEnts[0]);
        }

        if (shrubbery.treeBillboardEnts[0].isValid())
        {
            //TODO how do we handle partial treesets (ie < 4) where we mix billboards and models?
            shrubbery.treeBillboardEnts[0].getComponent<cro::Model>().setHidden(!shrubbery.treesetEnts.empty());
            shrubbery.treeBillboardEnts[1].getComponent<cro::Model>().setHidden(true);
        }

        for (auto e : shrubbery.treesetEnts)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Model>().setHidden(false);
            }
        }
        break;
    }
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

    size *= m_viewScale;
    m_croppingArea = { 6.f * m_viewScale.x, 6.f * m_viewScale.y, size.x - (12.f * m_viewScale.x), size.y - (12.f * m_viewScale.y) };
}

void PlaylistState::updateInfo()
{
    const std::string info =
        "Skybox: " + m_skyboxes[m_skyboxIndex] +
        "\nShrubs: " + m_shrubs[m_shrubIndex];

    m_infoEntity.getComponent<cro::Text>().setString(info);
}

void PlaylistState::quitState()
{
    //TODO some sort of confirmation, make sure everything is saved
    requestStackClear();
    requestStackPush(StateID::Menu);
}