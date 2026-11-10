/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2026
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "BushState.hpp"
#include "BushStateConsts.hpp"
#include "GrassProcessing.hpp"
#include "../golf/GameConsts.hpp"
#include "../golf/SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/audio/AudioMixer.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <Social.hpp>

namespace
{
#include "../golf/shaders/TreeShader.inl"
#include "../golf/shaders/ShaderIncludes.inl"

    struct BushShaderID final
    {
        enum
        {
            Bush,
            Branch
        };
    };
}

BushState::BushState(cro::StateStack& stack, cro::State::Context context, const SharedStateData& sd)
    : cro::State                (stack, context),
    m_sharedData                (sd),
    m_gameScene                 (context.appInstance.getMessageBus()),
    m_skyScene                  (context.appInstance.getMessageBus()),
    m_windBuffer                ("WindValues"),
    m_resolutionBuffer          ("ScaledResolution"),
    m_scaleBuffer               ("PixelScale"),
    m_lastTreeset               ("untitled.tst"),
    m_bushMaterial              (-1),
    m_branchMaterial            (-1),
    m_rotation                  (0.f),
    m_leafTexture               (nullptr),
    m_windRotation              (0.f),
    m_rootRotation              (0.f),
    m_billboardScaleMultiplier  (0.46f),
    m_skyMid                    (cro::Colour::White),
    m_skyTop                    (cro::Colour::CornflowerBlue),
    m_selectedCollisionObject   (0),
    m_openDialogueState         (OpenDialogueState::None),
    m_openFileBrowser           (ImGuiFileBrowserFlags_CloseOnEsc),
    m_saveDialogueState         (SaveDialogueState::None),
    m_saveFileBrowser           (ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_ConfirmOnEnter | ImGuiFileBrowserFlags_EnterNewFilename)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
    });

    std::fill(m_windowIndices.begin(), m_windowIndices.end(), 0);

    registerWindow(std::bind(&BushState::drawUI, this));
    registerWindows();

    m_palette.loadFromFile("assets/workshop/colordome-32.ase");

    if (cro::Console::isVisible())
    {
        cro::Console::show();
    }

    Social::setStatus(Social::InfoID::Menu, { "Treeset Editor" });
}

//public
bool BushState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_EVENT_KEY_UP)
    {
        switch (evt.key.key)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(0.f);
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 2.f));

                cro::AudioMixer::setPrefadeVolume(currTime, MixerChannel::Vehicles);

                if (currTime == 1)
                {
                    requestStackPop();
                    e.getComponent<cro::Callback>().active = false;
                }
            };
        }
            break;
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_WHEEL)
    {
        m_gameScene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 0.f, -evt.wheel.y });
    }
    else if (evt.type == SDL_EVENT_MOUSE_MOTION)
    {
        if (evt.motion.state & SDL_BUTTON_MMASK)
        {
            if (cro::Keyboard::isKeyPressed(SDLK_LSHIFT))
            {
                //rotate root
                m_rootRotation.y += static_cast<float>(evt.motion.xrel) / 80.f;
                //rootRotation.x += static_cast<float>(evt.motion.yrel) / 80.f;

                glm::mat4 rotation = glm::rotate(glm::mat4(1.f), m_rootRotation.y, cro::Transform::Y_AXIS);
                auto q = glm::quat_cast(glm::rotate(rotation, m_rootRotation.x, cro::Transform::X_AXIS));

                m_root.getComponent<cro::Transform>().setRotation(q);

                rotation = glm::rotate(glm::mat4(1.f), -m_rootRotation.y, cro::Transform::Y_AXIS);
                m_skyScene.getActiveCamera().getComponent<cro::Transform>().setRotation(glm::quat_cast(rotation));
            }
            else
            {
                //drag cam
                auto position = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getPosition();
                position.x -= static_cast<float>(evt.motion.xrel) / 50.f;
                position.y += static_cast<float>(evt.motion.yrel) / 50.f;

                m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition(position);
            }
        }
    }

    m_gameScene.forwardEvent(evt);
    m_skyScene.forwardEvent(evt);
    return false;
}

void BushState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_skyScene.forwardMessage(msg);
}

bool BushState::simulate(float dt)
{
    static constexpr float Speed = 4.f;
    glm::vec3 movement(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_W))
    {
        movement.z -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_S))
    {
        movement.z += dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_SPACE))
    {
        movement.y += dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_LCTRL))
    {
        movement.y -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_A))
    {
        movement.x -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_D))
    {
        movement.x += dt * Speed;
    }

    if (auto len2 = glm::length2(movement); len2 > 1)
    {
        movement /= std::sqrt(len2);
    }
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().move(movement);


    m_windData.elapsedTime += dt;
    m_windBuffer.setData(m_windData);

    m_gameScene.simulate(dt);
    m_skyScene.simulate(dt);
    return true;
}

void BushState::preRender()
{
    m_windBuffer.bind();
    m_resolutionBuffer.bind();
    m_scaleBuffer.bind();

    m_gameScene.preRender();

    glEnable(GL_PROGRAM_POINT_SIZE);

    glUseProgram(m_shaderUniform.shaderID);
    glUniform1f(m_shaderUniform.size, m_billboardScaleMultiplier * m_treeset.leafSize);
    auto oldCam = m_gameScene.setActiveCamera(m_billboardCamera);
    m_billboardTexture.clear(cro::Colour::Transparent);
    m_gameScene.render();
    m_billboardTexture.display();

    glUseProgram(m_shaderUniform.shaderID);
    glUniform1f(m_shaderUniform.size, m_treeset.leafSize);
    m_gameScene.setActiveCamera(oldCam);
}

void BushState::render()
{
    //renders a background over any other states which
    //might exist behind this one.
    m_backgroundQuad.draw();

    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_PROGRAM_POINT_SIZE);
    m_gameScene.render();
}

//private
void BushState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_skyScene.addSystem<cro::CallbackSystem>(mb);
    m_skyScene.addSystem<cro::CameraSystem>(mb);
    m_skyScene.addSystem<cro::ModelRenderer>(mb);
}

void BushState::loadAssets()
{
    m_resources.textures.setFallbackColour(cro::Colour::Black);
    auto& quadTex = m_resources.textures.get(std::numeric_limits<std::uint32_t>::max());
    m_backgroundQuad.setScale(cro::App::getWindow().getSize() / quadTex.getSize());
    m_backgroundQuad.setTexture(quadTex);

    m_scaleBuffer.setData(1.f);

    //std::string hq = m_sharedData.treeQuality == SharedStateData::High ? "#define HQ\n" : "";

    for (const auto& [name, str] : IncludeMappings)
    {
        m_resources.shaders.addInclude(name, str);
    }
    static const std::string MapSizeString = "const vec2 MapSize = vec2(" + std::to_string(MapSize.x) + ".0, " + std::to_string(MapSize.y) + ".0); ";
    m_resources.shaders.addInclude("MAP_SIZE", MapSizeString.c_str());



    m_resources.shaders.loadFromString(BushShaderID::Bush, BushVertex, BushFragment, "#define INSTANCING\n#define HQ\n");
    auto* shader = &m_resources.shaders.get(BushShaderID::Bush);
    m_bushMaterial = m_resources.materials.add(*shader);

    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_scaleBuffer.addShader(*shader);


    auto& texture = m_resources.textures.get("assets/golf/treesets/leaf06.png");
    texture.setSmooth(false);
    m_leafTexture = &texture;
    m_treeset.texturePath = "leaf06.png";

    auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);
    noiseTex.setSmooth(true);

    auto& material = m_resources.materials.get(m_bushMaterial);
    material.setProperty("u_diffuseMap", texture);
    material.setProperty("u_noiseTexture", noiseTex);

    m_shaderUniform.shaderID = shader->getGLHandle();
    m_shaderUniform.randomness = shader->getUniformID("u_randAmount");
    m_shaderUniform.size = shader->getUniformID("u_leafSize");
    m_shaderUniform.colour = shader->getUniformID("u_colour");
    m_shaderUniform.targetHeight = shader->getUniformID("u_targetHeight");
    m_shaderUniform.colourRotation = shader->getUniformID("u_rotation");

    m_resources.shaders.loadFromString(BushShaderID::Branch, BranchVertex, BranchFragment, "#define INSTANCING\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(BushShaderID::Branch);
    m_branchMaterial = m_resources.materials.add(*shader);
    m_resources.materials.get(m_branchMaterial).setProperty("u_noiseTexture", noiseTex);

    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_scaleBuffer.addShader(*shader);

    struct Transform final
    {
        Transform(glm::vec3 t, glm::vec3 s, float r)
            : position(t), scale(s), rotation(r) {}
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 scale = glm::vec3(1.f);
        float rotation = 0.f;
    };

    const std::array transforms =
    {
        Transform(glm::vec3(1.f, 0.f, 4.f), glm::vec3(1.f), 1.f),
        Transform(glm::vec3(-1.f, 0.f, 4.f), glm::vec3(1.2f), 3.f),
        Transform(glm::vec3(0.f, 0.f, -1.5f), glm::vec3(0.85f), 4.5f),
        Transform(glm::vec3(2.f, 0.f, -2.f), glm::vec3(0.5f), 5.5f),
        Transform(glm::vec3(-2.f, 0.f, -2.f), glm::vec3(1.85f), 2.5f),
        Transform(glm::vec3(0.f, 0.f, 5.f), glm::vec3(1.1f), 4.f),
    };

    for (const auto& t : transforms)
    {
        auto& mat = m_instanceTransforms.emplace_back(1.f);
        mat = glm::translate(mat, t.position);
        mat = glm::rotate(mat, t.rotation, cro::Transform::Y_AXIS);
        mat = glm::scale(mat, t.scale);
    }



    //used to render billboards from models
    m_billboardTexture.create(BillboardTargetSize.x, BillboardTargetSize.y);

    //used to render thumbnails of hole models
    m_thumbnailTexture.create(ThumbnailSize.x, ThumbnailSize.y);

    m_skyScene.enableSkybox();
    m_skyScene.setSkyboxColours(SkyBottom, m_skyMid, m_skyTop);
}

void BushState::createScene()
{
    //totally unnecessary fade out for audio :)
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::max(0.f, currTime - (dt * 2.f));

        cro::AudioMixer::setPrefadeVolume(currTime, MixerChannel::Vehicles);

        if (currTime == 0)
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };


    m_root = m_gameScene.createEntity();
    m_root.addComponent<cro::Transform>();


    //placeholder to help visialise player size
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/player_box.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        float offset = entity.getComponent<cro::Model>().getMeshData().boundingBox[0].y;
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, offset, 0.f });
        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlagsBillboard | RenderFlagsThumbnail));

        m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    if (md.loadFromFile("assets/models/ground_plane.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlagsBillboard | RenderFlagsThumbnail));

        m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }


    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&BushState::updateView, this, std::placeholders::_1);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(1024, 1024);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, ~RenderFlagsThumbnail);
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(20.f);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(10.f);
    camEnt.getComponent<cro::Camera>().setBlurPassCount(1);
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 6.f });

    auto sun = m_gameScene.getSunlight();
    sun.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 35.f * cro::Util::Const::degToRad);
    sun.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -35.f * cro::Util::Const::degToRad);



    //ortho cam for billboard view
    glm::vec2 camSize = glm::vec2(BillboardTargetSize) / PixelsPerMetre;
    m_billboardCamera = m_gameScene.createEntity();
    m_billboardCamera.addComponent<cro::Transform>().setPosition({0.f, 0.f, 3.f});
    m_billboardCamera.addComponent<cro::Camera>().setOrthographic(-(camSize.x) / 2.f, camSize.x / 2.f, 0.f, camSize.y, 0.1f, 16.f);
    m_billboardCamera.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlagsBillboard);


    //ortho cam for creating thumbnails
    m_thumbnailCamera = m_gameScene.createEntity();
    m_thumbnailCamera.addComponent<cro::Transform>().setPosition({ 0.f, 40.f, 0.f });
    m_thumbnailCamera.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    m_thumbnailCamera.addComponent<cro::Camera>().setOrthographic(0.f, MapSizeFloat.x, 0.f, MapSizeFloat.y, -0.1f, 50.f);
    m_thumbnailCamera.getComponent<cro::Camera>().viewport = { 0.f, 0.f, 1.f, 1.f };
    m_thumbnailCamera.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, RenderFlagsThumbnail);
}

void BushState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    //set this far distance to be similar to the game so we get the same sort of depth buffer
    cam3D.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 340.f); 
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    //set the correct renderable height in the shader
    auto targetHeight = static_cast<float>(cro::App::getWindow().getSize().y) * size.y;
    glUseProgram(m_shaderUniform.shaderID);
    glUniform1f(m_shaderUniform.targetHeight, targetHeight);

    ResolutionData rd;
    rd.resolution = cro::App::getWindow().getSize();
    rd.nearFadeDistance = 0.2f;
    m_resolutionBuffer.setData(rd);

    //resize the background quad
    //m_backgroundQuad.setScale(cro::App::getWindow().getSize() / glm::uvec2(m_backgroundQuad.getSize()));

    //and update skybox cam
    auto& skyCam = m_skyScene.getActiveCamera().getComponent<cro::Camera>();
    skyCam.viewport = cam3D.viewport;
    skyCam.setPerspective(cam3D.getFOV(), cam3D.getAspectRatio(), 0.5f, 14.f);
}

void BushState::registerWindows()
{
    auto& menuBar = m_windows.emplace_back();
    menuBar.visible = true;
    menuBar.draw = std::bind(&BushState::drawMenuBar, this, std::placeholders::_1);

    //we need to track these indices so we can use the menu bar etc to toggle visiblity
    m_windowIndices[WindowIndex::Properties] = static_cast<std::int32_t>(m_windows.size());
    auto& properties = m_windows.emplace_back();
    properties.visible = true;
    properties.draw = std::bind(&BushState::drawProperties, this, std::placeholders::_1);

    m_windowIndices[WindowIndex::Billboard] = static_cast<std::int32_t>(m_windows.size());
    auto& billboard = m_windows.emplace_back();
    billboard.draw = std::bind(&BushState::drawBillboard, this, std::placeholders::_1);

    m_windowIndices[WindowIndex::Thumbnails] = static_cast<std::int32_t>(m_windows.size());
    auto& thumbnail = m_windows.emplace_back();
    thumbnail.draw = std::bind(&BushState::drawThumbnail, this, std::placeholders::_1);

    m_windowIndices[WindowIndex::Skybox] = static_cast<std::int32_t>(m_windows.size());
    auto& skybox = m_windows.emplace_back();
    skybox.draw = std::bind(&BushState::drawSkyboxEditor, this, std::placeholders::_1);

    loadWindowStates();
}

void BushState::drawUI()
{
    for (auto& window : m_windows)
    {
        if (window.visible)
        {
            window.draw(&window.visible);
        }
    }
}

void BushState::loadModel(const std::filesystem::path& path)
{
    if (m_models[0].isValid())
    {
        m_gameScene.destroyEntity(m_models[0]);
        m_gameScene.destroyEntity(m_models[1]);
        m_gameScene.destroyEntity(m_models[2]);
        m_rotation = 0.f;
        m_materials.clear();

        for (auto& o : m_collisionObjects)
        {
            m_gameScene.destroyEntity(o.preview);
        }
        m_collisionObjects.clear();
    }


    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile(path))
    {
        //base model on left
        cro::Entity entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlagsBillboard | RenderFlagsThumbnail));
        m_models[0] = entity;

        float radius = m_models[0].getComponent<cro::Model>().getMeshData().boundingSphere.radius;
        radius *= 1.1f;
        entity.getComponent<cro::Transform>().setPosition({ -radius, 0.f, 0.f });
        m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        m_materials.resize(entity.getComponent<cro::Model>().getMeshData().submeshCount);

        //same model with bush shader (instanced)
        md.loadFromFile(path, true);

        for (auto i = 1u; i < m_models.size(); ++i)
        {
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec3(m_treeset.scale));
            md.createModel(entity);
            entity.getComponent<cro::Model>().setRenderFlags(~RenderFlagsThumbnail);

            m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            entity.getComponent<cro::Transform>().setPosition({ radius, 0.f, 0.f });


            auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

            //set branch as default
            for (auto j = 0u; j < meshData->submeshCount; ++j)
            {
                auto mat = m_resources.materials.get(m_branchMaterial);
                mat.doubleSided = true;
                if (md.getMaterial(j)->properties.count("u_diffuseMap"))
                {
                    cro::TextureID tid(md.getMaterial(j)->properties.at("u_diffuseMap").second.textureID);
                    mat.setProperty("u_diffuseMap", tid);
                }
                entity.getComponent<cro::Model>().setMaterial(j, mat);

                m_materials[j].materials[0] = mat;
                m_materials[j].materials[1] = m_resources.materials.get(m_bushMaterial);
                m_materials[j].materials[1].doubleSided = true;
            }

            m_models[i] = entity;
        }

        m_models[1].getComponent<cro::Model>().setInstanceTransforms({ glm::mat4(1.f) });
        m_models[2].getComponent<cro::Model>().setInstanceTransforms(m_instanceTransforms);
        m_models[2].getComponent<cro::Model>().setHidden(true);

        m_treeset.modelPath = cro::FileSystem::getFileName(path);

        m_billboardCamera.getComponent<cro::Transform>().setPosition({ radius, 0.f, 9.f });
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed opening model");
        m_treeset = {};
    }
}

void BushState::loadPreset(const std::filesystem::path& path)
{
    const auto workingDir = cro::FileSystem::getFilePath(path);
    m_treeset.scale = 1.f;

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        m_treeset = {};

        std::vector<std::uint32_t> branchIndices;
        std::vector<std::uint32_t> leafIndices;

        std::vector<CollisionObject> collisionObjects;

        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                m_treeset.modelPath = p.getValue<std::string>();
            }
            else if (name == "texture")
            {
                m_treeset.texturePath = p.getValue<std::string>();
            }
            else if (name == "colour")
            {
                m_treeset.colour = p.getValue<glm::vec3>();
                for (auto i = 0; i < 3; ++i)
                {
                    m_treeset.colour[i] = std::min(1.f, std::max(0.f, m_treeset.colour[i]));
                }
            }
            else if (name == "colour_rotation")
            {
                m_treeset.colourRotation = std::clamp(p.getValue<float>(), 0.f, 1.f);
            }
            else if (name == "randomness")
            {
                m_treeset.randomAmount = std::max(0.02f, std::min(1.f, p.getValue<float>()));
            }
            else if (name == "leaf_size")
            {
                m_treeset.leafSize = std::max(0.01f, std::min(1.f, p.getValue<float>()));
            }
            else if (name == "branch_index")
            {
                branchIndices.push_back(p.getValue<std::uint32_t>());
            }
            else if (name == "leaf_index")
            {
                leafIndices.push_back(p.getValue<std::uint32_t>());
            }
            else if (name == "scale")
            {
                m_treeset.scale = std::max(MinModelScale, p.getValue<float>());
            }
        }

        //NOTE this clears collision objects so they need to be loaded afterwards!
        loadModel(workingDir / m_treeset.modelPath);
        m_leafTexture->loadFromFile(workingDir / m_treeset.texturePath);

        LogI << FILE_LINE << " parse collision objects here" << std::endl;

        if (!collisionObjects.empty())
        {
            collisionObjects.swap(m_collisionObjects);
        }

        for (auto& o : m_collisionObjects)
        {
            LogI << FILE_LINE << " create collision preview here" << std::endl;
        }

        //look at material slots and apply correct material to submesh
        for (auto b : branchIndices)
        {
            if (b < m_materials.size())
            {
                m_materials[b].activeMaterial = 0;
            }
        }

        for (auto l : leafIndices)
        {
            if (l < m_materials.size())
            {
                m_materials[l].activeMaterial = 1;
            }
        }


        for (auto j = 1u; j < m_models.size(); ++j)
        {
            for (auto i = 0u; i < m_materials.size(); ++i)
            {
                m_models[j].getComponent<cro::Model>().setMaterial(i, m_materials[i].materials[m_materials[i].activeMaterial]);
                m_models[j].getComponent<cro::Model>().getMeshData().indexData[i].primitiveType =
                    m_materials[i].activeMaterial ? GL_POINTS : GL_TRIANGLES;
            }
        }


        glUseProgram(m_shaderUniform.shaderID);
        glUniform1f(m_shaderUniform.randomness, m_treeset.randomAmount);
        glUniform1f(m_shaderUniform.size, m_treeset.leafSize);
        glUniform3f(m_shaderUniform.colour, m_treeset.colour.r, m_treeset.colour.g, m_treeset.colour.b);
        glUniform1f(m_shaderUniform.colourRotation, m_treeset.colourRotation);

        m_lastTreeset = path;
    }
}

void BushState::savePreset(const std::filesystem::path& path)
{
    cro::ConfigFile cfg("m_treeset");
    cfg.addProperty("model").setValue(U8PATH_CAST(m_treeset.modelPath));
    cfg.addProperty("texture").setValue(U8PATH_CAST(m_treeset.texturePath));
    cfg.addProperty("colour").setValue(m_treeset.colour);
    cfg.addProperty("colour_rotation").setValue(m_treeset.colourRotation);
    cfg.addProperty("randomness").setValue(m_treeset.randomAmount);
    cfg.addProperty("leaf_size").setValue(m_treeset.leafSize);
    cfg.addProperty("scale").setValue(m_treeset.scale);

    for (auto i = 0u; i < m_materials.size(); ++i)
    {
        if (m_materials[i].activeMaterial == 0)
        {
            cfg.addProperty("branch_index").setValue(i);
        }
        else
        {
            cfg.addProperty("leaf_index").setValue(i);
        }
    }

    LogI << FILE_LINE << " export collision objects here" << std::endl;

    if (cfg.save(path))
    {
        m_lastTreeset = path;
    }
}

void BushState::loadSkyboxFile()
{
    const auto path = cro::FileSystem::openFileDialogue("", "sbf");
    if (!path.empty())
    {
        auto ents = m_skyScene.getSystem<cro::ModelRenderer>()->getEntities();
        for (auto e : ents)
        {
            m_skyScene.destroyEntity(e);
        }

        SkyboxMaterials t;//not actually used
        loadSkybox(path, m_skyScene, m_resources, t);
        const auto& colours = m_skyScene.getSkyboxColours();
        m_skyMid = colours.middle;
        m_skyTop = colours.top;
    }
}

void BushState::saveSkyboxFile()
{
    const auto path = cro::FileSystem::saveFileDialogue("assets/golf/skyboxes" /*/ m_lastSkybox*/, "sbf");
    if (!path.empty())
    {
        cro::ConfigFile cfg;
        cfg.addProperty("sky_top").setValue(m_skyTop);
        cfg.addProperty("sky_bottom").setValue(m_skyMid);

        //TODO add path to cloud sprite sheet

        std::filesystem::path modelPath = "assets/golf/models/skybox/";
        auto ents = m_skyScene.getSystem<cro::ModelRenderer>()->getEntities();
        for (auto e : ents)
        {
            if (!e.getLabel().empty())
            {
                //TODO need to be able to rotate the models around Y
                glm::vec3 pos = e.getComponent<cro::Transform>().getPosition();
                float rotation = glm::eulerAngles(e.getComponent<cro::Transform>().getRotation()).y * cro::Util::Const::radToDeg;
                glm::vec3 scale = e.getComponent<cro::Transform>().getScale();

                auto* prop = cfg.addObject("prop");
                prop->addProperty("model").setValue(U8PATH_CAST((modelPath / e.getLabel())));
                prop->addProperty("position").setValue(pos);
                prop->addProperty("rotation").setValue(rotation);
                prop->addProperty("scale").setValue(scale);
            }
        }

        cfg.save(path);
    }
}

void BushState::addSkyboxModel()
{
    const auto path = cro::FileSystem::openFileDialogue("", "cmt");
    if (!path.empty())
    {
        cro::ModelDefinition md(m_resources);
        if (md.loadFromFile(path))
        {
            auto entity = m_skyScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.setLabel(U8PATH_CAST(cro::FileSystem::getFileName(path)));
            md.createModel(entity);
        }
    }
}

void BushState::createThumbnails()
{
    if (!cro::FileSystem::directoryExists("assets/golf/thumbs"))
    {
        LogI << "creating directory..." << std::endl;
        cro::FileSystem::createDirectory("assets/golf/thumbs");
    }

    std::vector<std::filesystem::path> inPaths;
    std::vector<std::filesystem::path> outPaths;

    //TODO a better predicate than converting to string
    auto dirs = cro::FileSystem::listDirectories("assets/golf/courses");
    dirs.erase(std::remove_if(dirs.begin(), dirs.end(), [](const std::filesystem::path& s) {return s.u8string().find(u8"course_") == std::u8string::npos; }), dirs.end());

    for (const auto& dir : dirs)
    {
        auto path = "assets/golf/courses/" / dir / "course.data";
        if (cro::FileSystem::fileExists(path))
        {
            inPaths.push_back(path);

            path = "assets/golf/thumbs" / dir;
            if (!cro::FileSystem::directoryExists(path))
            {
                LogI << "creating directory " << dir << "..." << std::endl;
                cro::FileSystem::createDirectory(path);
            }
            outPaths.push_back(path);

            path += "hires/";
            if (!cro::FileSystem::directoryExists(path))
            {
                LogI << "creating directory " << dir << "..." << std::endl;
                cro::FileSystem::createDirectory(path);
            }
        }
    }

    std::array<std::uint32_t, 3*6> flagBytes =
    {
        0xfff8e1ff, 0x00000000, 0x00000000,
        0xfff8e1ff, 0x00000000, 0x00000000,
        0xfff8e1ff, 0x00000000, 0x00000000,
        0xfff8e1ff, 0x00000000, 0x00000000,
        0xfff8e1ff, 0xb83530ff, 0xb83530ff,
        0xfff8e1ff, 0xb83530ff, 0xb83530ff,
    };
    cro::Image img;
    img.loadFromMemory(reinterpret_cast<const uint8_t*>(flagBytes.data()), 3, 6, cro::ImageFormat::RGBA);
    cro::Texture tex;
    tex.loadFromImage(img);
    cro::SimpleQuad flagQuad(tex);

    cro::RenderTexture bufferTexture;
    bufferTexture.create(ThumbnailSize.x, ThumbnailSize.y);
    cro::SimpleQuad bufferQuad(bufferTexture.getTexture());

    cro::RenderTexture bufferTextureHigh;
    bufferTextureHigh.create(ThumbnailSize.x * 8, ThumbnailSize.y * 8);

    auto unlitShader = m_resources.shaders.loadBuiltIn(cro::ShaderResource::BuiltIn::Unlit, cro::ShaderResource::BuiltInFlags::DiffuseMap);
    auto matID = m_resources.materials.add(m_resources.shaders.get(unlitShader));
    auto material = m_resources.materials.get(matID);

    static constexpr glm::vec3 MapCentre(MapSizeFloat.x / 2.f, 0.f, -MapSizeFloat.y / 2.f);

    cro::ModelDefinition md(m_resources);
    auto oldCam = m_gameScene.setActiveCamera(m_thumbnailCamera);
    for(auto i = 0u; i < inPaths.size(); ++i)
    {
        const auto& inPath = inPaths[i];
        const auto& outPath = outPaths[i];

        cro::ConfigFile cfg;
        if (cfg.loadFromFile(inPath))
        {
            std::vector<std::string> holes;
            const auto& props = cfg.getProperties();
            for (const auto& p : props)
            {
                if (p.getName() == "hole")
                {
                    holes.push_back(p.getValue<std::string>());
                }
            }

            for (const auto& hole : holes)
            {
                cro::ConfigFile holeFile;
                holeFile.loadFromFile(hole);

                if (auto* model = holeFile.findProperty("model"); model != nullptr)
                {
                    auto modelPath = model->getValue<std::string>();

                    if (md.loadFromFile(modelPath))
                    {
                        auto entity = m_gameScene.createEntity();
                        entity.addComponent<cro::Transform>().setPosition(MapCentre);
                        md.createModel(entity);
                        entity.getComponent<cro::Model>().setRenderFlags(RenderFlagsThumbnail);

                        //because we've started using material colours to affect
                        //the in-game material properties, we need to reset the
                        //material to the default unlit.
                        for (auto j = 0u; j < entity.getComponent<cro::Model>().getMeshData().submeshCount; ++j)
                        {
                            applyMaterialData(md, material, j);
                            entity.getComponent<cro::Model>().setMaterial(j, material);
                        }


                        const auto& bounds = entity.getComponent<cro::Model>().getMeshData().boundingBox;
                        auto o = bounds.getCentre();
                        o.y = 0.f;
                        entity.getComponent<cro::Transform>().setOrigin(o);

                        auto size = bounds[1] - bounds[0];
                        //size.z *= -1.f;

                        const float scaleX = /*std::max(1.f, std::floor*/(MapSizeFloat.x / size.x);//);
                        const float scaleY = /*std::max(1.f, std::floor*/(MapSizeFloat.y / size.z);//);

                        const float scale = std::min(scaleX, scaleY);
                        entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

                        glm::vec3 flagPos = glm::vec3(0.f);
                        auto* flag = holeFile.findProperty("pin");
                        if (flag)
                        {
                            flagPos = flag->getValue<glm::vec3>();

                            flagPos *= scale;
                            flagPos -= o * scale;
                            flagPos += MapCentre;

                            flagQuad.setPosition({ flagPos.x / (MapSizeFloat.x / ThumbnailSize.x), -flagPos.z / (MapSizeFloat.y / ThumbnailSize.y)});
                        }



                        m_gameScene.simulate(0.f);

                        bufferTexture.clear(cro::Colour::Transparent);
                        m_gameScene.render();
                        bufferTexture.display();

                        bufferTextureHigh.clear(WaterColour);
                        m_gameScene.render();
                        bufferTextureHigh.display();

                        m_thumbnailTexture.clear(/*cro::Colour(std::uint8_t(39), 56, 153)*/cro::Colour::Transparent);
                        bufferQuad.setPosition({ 2.f, -2.f });
                        bufferQuad.setColour(DropShadowColour); //constify this - same as minimap in GolfStateUI.cpp
                        bufferQuad.draw();
                        bufferQuad.setPosition({ 0.f, 0.f });
                        bufferQuad.setColour(cro::Colour::White);
                        bufferQuad.draw();
                        flagQuad.draw();
                        m_thumbnailTexture.display();

                        std::string fileName = U8PATH_CAST(cro::FileSystem::getFileName(hole));
                        fileName = fileName.substr(0, fileName.find_last_of('.'));
                        m_thumbnailTexture.saveToFile(outPath / fileName / ".png");

                        bufferTextureHigh.saveToFile(outPath / "hires" / fileName / ".png");

                        m_gameScene.destroyEntity(entity);
                        m_gameScene.simulate(0.f);
                    }
                }
            }
        }

    }
    m_gameScene.setActiveCamera(oldCam);

    cro::App::getInstance().resetFrameTime();
}