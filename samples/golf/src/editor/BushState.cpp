/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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
#include "../golf/GameConsts.hpp"
#include "../golf/SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/graphics/Palette.hpp>

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


    //TODO tidy all these up
    std::int32_t bushMaterial = -1;
    std::int32_t branchMaterial = -1;

    struct TreesetData final
    {
        float leafSize = 0.25f; //metres
        float randomAmount = 0.2f;
        glm::vec3 colour = glm::vec3(1.f);

        std::string modelPath;
        std::string texturePath;

    }treeset;
    std::string lastTreeset = "untitled.tst";
    std::string lastSkybox = "untitled.sbf";

    struct ShaderUniforms final
    {
        std::uint32_t shaderID = 0;
        std::int32_t size = -1;
        std::int32_t randomness = -1;
        std::int32_t colour = -1;
        std::int32_t targetHeight = -1;
    }shaderUniform;

    float rotation = 0.f;
    cro::Texture* leafTexture = nullptr;

    WindData windData;
    float windRotation = 0.f;

    cro::Palette palette;

    glm::vec2 rootRotation = glm::vec2(0.f);

    //we'll assume 64px per metre
    constexpr float PixelsPerMetre = 64.f;
    const glm::uvec2 BillboardTargetSize(640, 448);
    float billboardScaleMultiplier = 0.46f;


    std::uint64_t RenderFlagsBillboard = 1;
    std::uint64_t RenderFlagsThumbnail = 2;

    cro::Colour skyMid = cro::Colour::White;
    cro::Colour skyTop = cro::Colour::CornflowerBlue;

    constexpr glm::uvec2 ThumbnailSize(120u, 75u);
}

BushState::BushState(cro::StateStack& stack, cro::State::Context context, const SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_skyScene          (context.appInstance.getMessageBus()),
    m_windBuffer        ("WindValues"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_scaleBuffer       ("PixelScale"),
    m_editSkybox        (false)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
    });

    registerWindow([&]() 
        {
            drawUI();
        });

    palette.loadFromFile("assets/workshop/colordome-32.ase");

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

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
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
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        m_gameScene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 0.f, -evt.wheel.y });
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        if (evt.motion.state & SDL_BUTTON_MMASK)
        {
            if (cro::Keyboard::isKeyPressed(SDLK_LSHIFT))
            {
                //rotate root
                rootRotation.y += static_cast<float>(evt.motion.xrel) / 80.f;
                //rootRotation.x += static_cast<float>(evt.motion.yrel) / 80.f;

                glm::mat4 rotation = glm::rotate(glm::mat4(1.f), rootRotation.y, cro::Transform::Y_AXIS);
                auto q = glm::quat_cast(glm::rotate(rotation, rootRotation.x, cro::Transform::X_AXIS));

                m_root.getComponent<cro::Transform>().setRotation(q);

                rotation = glm::rotate(glm::mat4(1.f), -rootRotation.y, cro::Transform::Y_AXIS);
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
    if (cro::Keyboard::isKeyPressed(SDLK_w))
    {
        movement.z -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_s))
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
    if (cro::Keyboard::isKeyPressed(SDLK_a))
    {
        movement.x -= dt * Speed;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_d))
    {
        movement.x += dt * Speed;
    }

    if (auto len2 = glm::length2(movement); len2 > 1)
    {
        movement /= std::sqrt(len2);
    }
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().move(movement);


    windData.elapsedTime += dt;
    m_windBuffer.setData(windData);

    m_gameScene.simulate(dt);
    m_skyScene.simulate(dt);
    return true;
}

void BushState::render()
{
    m_windBuffer.bind(0);
    m_resolutionBuffer.bind(1);
    m_scaleBuffer.bind(2);

    glEnable(GL_PROGRAM_POINT_SIZE);


    glUseProgram(shaderUniform.shaderID);
    glUniform1f(shaderUniform.size, billboardScaleMultiplier * treeset.leafSize);
    auto oldCam = m_gameScene.setActiveCamera(m_billboardCamera);
    m_billboardTexture.clear(cro::Colour::Transparent);
    m_gameScene.render();
    m_billboardTexture.display();

    glUseProgram(shaderUniform.shaderID);
    glUniform1f(shaderUniform.size, treeset.leafSize);
    m_gameScene.setActiveCamera(oldCam);
    m_backgroundQuad.draw();


    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
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

    m_resources.shaders.loadFromString(BushShaderID::Bush, BushVertex, BushFragment, "#define INSTANCING\n#define HQ\n");
    auto* shader = &m_resources.shaders.get(BushShaderID::Bush);
    bushMaterial = m_resources.materials.add(*shader);

    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_scaleBuffer.addShader(*shader);

    auto& texture = m_resources.textures.get("assets/golf/treesets/leaf06.png");
    texture.setSmooth(false);
    leafTexture = &texture;
    treeset.texturePath = "leaf06.png";

    auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);
    noiseTex.setSmooth(true);

    auto& material = m_resources.materials.get(bushMaterial);
    material.setProperty("u_diffuseMap", texture);
    material.setProperty("u_noiseTexture", noiseTex);

    shaderUniform.shaderID = shader->getGLHandle();
    shaderUniform.randomness = shader->getUniformID("u_randAmount");
    shaderUniform.size = shader->getUniformID("u_leafSize");
    shaderUniform.colour = shader->getUniformID("u_colour");
    shaderUniform.targetHeight = shader->getUniformID("u_targetHeight");

    m_resources.shaders.loadFromString(BushShaderID::Branch, BranchVertex, BranchFragment, "#define INSTANCING\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(BushShaderID::Branch);
    branchMaterial = m_resources.materials.add(*shader);
    m_resources.materials.get(branchMaterial).setProperty("u_noiseTexture", noiseTex);

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
    m_skyScene.setSkyboxColours(SkyBottom, skyMid, skyTop);
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
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(2048, 2048);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, ~RenderFlagsThumbnail);
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(20.f);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(10.f);
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
    m_thumbnailCamera.addComponent<cro::Camera>().setOrthographic(0.f, 320.f, 0.f, 200.f, -0.1f, 50.f);
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
    glUseProgram(shaderUniform.shaderID);
    glUniform1f(shaderUniform.targetHeight, targetHeight);

    ResolutionData rd;
    rd.resolution = cro::App::getWindow().getSize();
    rd.nearFadeDistance = 0.2f;
    m_resolutionBuffer.setData(rd);

    //resize the background quad
    m_backgroundQuad.setScale(cro::App::getWindow().getSize() / glm::uvec2(m_backgroundQuad.getSize()));

    //and update skybox cam
    auto& skyCam = m_skyScene.getActiveCamera().getComponent<cro::Camera>();
    skyCam.viewport = cam3D.viewport;
    skyCam.setPerspective(cam3D.getFOV(), cam3D.getAspectRatio(), 0.5f, 14.f);
}

void BushState::drawUI()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load Model"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "cmt");
                if (!path.empty())
                {
                    loadModel(path);
                }
            }
            if (ImGui::MenuItem("Open Preset"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "tst");
                if (!path.empty())
                {
                    loadPreset(path);
                }
            }
            if (ImGui::MenuItem("Save Preset"))
            {
                auto path = cro::FileSystem::saveFileDialogue(lastTreeset, "tst");
                if (!path.empty())
                {
                    savePreset(path);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
            {
                if (getStateCount() == 1)
                {
                    cro::App::quit();
                }
                else
                {
                    requestStackPop();
                    Social::setStatus(Social::InfoID::Menu, { "Main Menu" });
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Skybox Editor", nullptr, &m_editSkybox);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Create Thumbs")
                && cro::FileSystem::showMessageBox("Are You Sure?", "This may take some time, and window may\nbecome unresponsive. Please be\npatient.", cro::FileSystem::ButtonType::YesNo))
            {
                createThumbnails();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (ImGui::Begin("Properties"))
    {
        if (ImGui::SliderFloat("Leaf Size", &treeset.leafSize, 0.01f, 1.f))
        {
            glUseProgram(shaderUniform.shaderID);
            glUniform1f(shaderUniform.size, treeset.leafSize);
        }

        if (ImGui::SliderFloat("Randomness", &treeset.randomAmount, 0.f, 1.f))
        {
            glUseProgram(shaderUniform.shaderID);
            glUniform1f(shaderUniform.randomness, treeset.randomAmount);
        }

        if (ImGui::ColorEdit3("Colour", &treeset.colour[0]))
        {
            glUseProgram(shaderUniform.shaderID);
            glUniform3f(shaderUniform.colour, treeset.colour.r, treeset.colour.g, treeset.colour.b);
        }

        if (ImGui::Button("Load Palette"))
        {
            auto path = cro::FileSystem::openFileDialogue("", "ase");
            if (!path.empty())
            {
                palette.loadFromFile(path);
            }
        }
        auto i = 0;
        for (const auto& swatch : palette.getSwatches())
        {
            for (const auto& colour : swatch.colours)
            {
                ImVec4 c(colour.colour.getVec4());
                std::string label = colour.name.toAnsiString() + "##" + std::to_string(i);
                if (ImGui::ColorButton(label.c_str(), c))
                {
                    treeset.colour = { c.x, c.y, c.z };
                    glUseProgram(shaderUniform.shaderID);
                    glUniform3f(shaderUniform.colour, c.x, c.y, c.z);
                }

                if ((i++ % 12) != 11)
                {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::NewLine();
        ImGui::Separator();
        ImGui::SliderFloat("Wind Strength", &windData.direction[1], 0.1f, 1.f);
        if (ImGui::SliderFloat("Wind Direction", &windRotation, 0.f, cro::Util::Const::PI))
        {
            auto dir = glm::rotate(glm::quat(0.f, 0.f, 0.f, 1.f), windRotation, cro::Transform::Y_AXIS) * glm::vec3(-1.f, 0.f, 0.f);
            windData.direction[0] = dir.x;
            windData.direction[2] = dir.z;
        }

        if (ImGui::Button("Load Texture"))
        {
            auto path = cro::FileSystem::openFileDialogue("", "png");
            if (!path.empty())
            {
                leafTexture->loadFromFile(path);
                treeset.texturePath = cro::FileSystem::getFileName(path);
            }
        }
        ImGui::SameLine();
        ImGui::Image(*leafTexture, { 32.f, 32.f }, { 0.f, 0.f }, { 1.f, 1.f });


        ImGui::NewLine();
        if (m_models[0].isValid())
        {
            if (ImGui::SliderFloat("Rotation", &rotation, 0.f, cro::Util::Const::TAU))
            {
                for (auto model : m_models)
                {
                    model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
                }
            }

            static bool showInstanced = false;
            if (ImGui::Checkbox("Instanced", &showInstanced))
            {
                m_models[1].getComponent<cro::Model>().setHidden(showInstanced);
                m_models[2].getComponent<cro::Model>().setHidden(!showInstanced);
            }

            for (i = 0u; i < m_materials.size(); ++i)
            {
                auto label = "Material##" + std::to_string(i);
                if (ImGui::InputInt(label.c_str(), &m_materials[i].activeMaterial))
                {
                    m_materials[i].activeMaterial = std::min(1, std::max(0, m_materials[i].activeMaterial));

                    for (auto j = 1u; j < m_models.size(); ++j)
                    {
                        auto primitiveType = m_sharedData.treeQuality == SharedStateData::High ? GL_POINTS : GL_TRIANGLES;

                        m_models[j].getComponent<cro::Model>().setMaterial(i, m_materials[i].materials[m_materials[i].activeMaterial]);
                        m_models[j].getComponent<cro::Model>().getMeshData().indexData[i].primitiveType =
                            m_materials[i].activeMaterial ? primitiveType : GL_TRIANGLES;
                    }
                }
            }
        }

    }
    ImGui::End();

    if (ImGui::Begin("Billboard"))
    {
        glm::vec2 size(BillboardTargetSize);
        ImGui::Image(m_billboardTexture.getTexture(), {size.x, size.y}, {0.f, 1.f}, {1.f, 0.f});

        ImGui::SliderFloat("Leaf Scale", &billboardScaleMultiplier, 0.1f, 1.f);

        if (ImGui::Button("Reset Rotation"))
        {
            rootRotation = glm::vec2(0.f);
            m_root.getComponent<cro::Transform>().setRotation(glm::mat4(1.f));
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Image"))
        {
            auto path = cro::FileSystem::saveFileDialogue("", "png");
            if (!path.empty())
            {
                m_billboardTexture.saveToFile(path);
            }
        }
    }
    ImGui::End();

    if (ImGui::Begin("Thumbnail"))
    {
        ImGui::Image(m_thumbnailTexture.getTexture(), { static_cast<float>(ThumbnailSize.x), static_cast<float>(ThumbnailSize.y) }, { 0.f, 1.f }, { 1.f, 0.f });
    }
    ImGui::End();

    if (m_editSkybox)
    {
        if (ImGui::Begin("Skybox"))
        {
            if (ImGui::Button("Load Skybox"))
            {
                loadSkyboxFile();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Skybox"))
            {
                saveSkyboxFile();
            }

            //hacky but should be ok if there are not too many models in
            //the skybox scene.
            std::vector<cro::Entity> items;
            for (auto e : m_skyScene.getSystem<cro::ModelRenderer>()->getEntities())
            {
                if (!e.getLabel().empty())
                {
                    items.push_back(e);
                }
            }
            int k = 0;
            for (auto item : items)
            {
                ImGui::Text("%s", item.getLabel().c_str());
                ImGui::SameLine();
                std::string label = "Remove##" + std::to_string(k++);
                if (ImGui::Button(label.c_str()))
                {
                    m_skyScene.destroyEntity(item);
                }
            }
            if (ImGui::Button("Add Model"))
            {
                addSkyboxModel();
            }
            ImGui::NewLine();

            auto i = 0;
            ImGui::Text("Sky Top");
            for (const auto& swatch : palette.getSwatches())
            {
                for (const auto& colour : swatch.colours)
                {
                    ImVec4 c(colour.colour.getVec4());
                    std::string label = colour.name.toAnsiString() + "##" + std::to_string(i);
                    if (ImGui::ColorButton(label.c_str(), c))
                    {
                        skyTop = cro::Colour(c.x, c.y, c.z);
                        m_skyScene.setSkyboxColours(SkyBottom, skyMid, skyTop);
                    }

                    if ((i++ % 12) != 11)
                    {
                        ImGui::SameLine();
                    }
                }
            }
            ImGui::Separator();
            i = 120;
            ImGui::Text("Sky Bottom");
            for (const auto& swatch : palette.getSwatches())
            {
                for (const auto& colour : swatch.colours)
                {
                    ImVec4 c(colour.colour.getVec4());
                    std::string label = colour.name.toAnsiString() + "##" + std::to_string(i);
                    if (ImGui::ColorButton(label.c_str(), c))
                    {
                        skyMid = cro::Colour(c.x, c.y, c.z);
                        m_skyScene.setSkyboxColours(SkyBottom, skyMid, skyTop);
                    }

                    if ((i++ % 12) != 11)
                    {
                        ImGui::SameLine();
                    }
                }
            }
        }
        ImGui::End();
    }
}

void BushState::loadModel(const std::string& path)
{
    if (m_models[0].isValid())
    {
        m_gameScene.destroyEntity(m_models[0]);
        m_gameScene.destroyEntity(m_models[1]);
        m_gameScene.destroyEntity(m_models[2]);
        rotation = 0.f;
        m_materials.clear();
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
            entity.addComponent<cro::Transform>();
            md.createModel(entity);
            entity.getComponent<cro::Model>().setRenderFlags(~RenderFlagsThumbnail);

            m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            entity.getComponent<cro::Transform>().setPosition({ radius, 0.f, 0.f });


            auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

            //set branch as default
            for (auto j = 0u; j < meshData->submeshCount; ++j)
            {
                auto mat = m_resources.materials.get(branchMaterial);
                mat.doubleSided = true;
                if (md.getMaterial(j)->properties.count("u_diffuseMap"))
                {
                    cro::TextureID tid(md.getMaterial(j)->properties.at("u_diffuseMap").second.textureID);
                    mat.setProperty("u_diffuseMap", tid);
                }
                entity.getComponent<cro::Model>().setMaterial(j, mat);

                m_materials[j].materials[0] = mat;
                m_materials[j].materials[1] = m_resources.materials.get(bushMaterial);
                m_materials[j].materials[1].doubleSided = true;
            }

            m_models[i] = entity;
        }

        m_models[1].getComponent<cro::Model>().setInstanceTransforms({ glm::mat4(1.f) });
        m_models[2].getComponent<cro::Model>().setInstanceTransforms(m_instanceTransforms);
        m_models[2].getComponent<cro::Model>().setHidden(true);

        treeset.modelPath = cro::FileSystem::getFileName(path);

        m_billboardCamera.getComponent<cro::Transform>().setPosition({ radius, 0.f, 9.f });
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed opening model");
        treeset = {};
    }
}

void BushState::loadPreset(const std::string& path)
{
    auto workingDir = cro::FileSystem::getFilePath(path);

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        treeset = {};

        std::vector<std::uint32_t> branchIndices;
        std::vector<std::uint32_t> leafIndices;

        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "model")
            {
                treeset.modelPath = p.getValue<std::string>();
            }
            else if (name == "texture")
            {
                treeset.texturePath = p.getValue<std::string>();
            }
            else if (name == "colour")
            {
                treeset.colour = p.getValue<glm::vec3>();
                for (auto i = 0; i < 3; ++i)
                {
                    treeset.colour[i] = std::min(1.f, std::max(0.f, treeset.colour[i]));
                }
            }
            else if (name == "randomness")
            {
                treeset.randomAmount = std::max(0.02f, std::min(1.f, p.getValue<float>()));
            }
            else if (name == "leaf_size")
            {
                treeset.leafSize = std::max(0.01f, std::min(1.f, p.getValue<float>()));
            }
            else if (name == "branch_index")
            {
                branchIndices.push_back(p.getValue<std::uint32_t>());
            }
            else if (name == "leaf_index")
            {
                leafIndices.push_back(p.getValue<std::uint32_t>());
            }
        }

        loadModel(workingDir + treeset.modelPath);
        leafTexture->loadFromFile(workingDir + treeset.texturePath);

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
                auto primitiveType = m_sharedData.treeQuality == SharedStateData::High ? GL_POINTS : GL_TRIANGLES;

                m_models[j].getComponent<cro::Model>().setMaterial(i, m_materials[i].materials[m_materials[i].activeMaterial]);
                m_models[j].getComponent<cro::Model>().getMeshData().indexData[i].primitiveType =
                    m_materials[i].activeMaterial ? primitiveType : GL_TRIANGLES;
            }
        }


        glUseProgram(shaderUniform.shaderID);
        glUniform1f(shaderUniform.randomness, treeset.randomAmount);
        glUniform1f(shaderUniform.size, treeset.leafSize);
        glUniform3f(shaderUniform.colour, treeset.colour.r, treeset.colour.g, treeset.colour.b);

        lastTreeset = path;
    }
}

void BushState::savePreset(const std::string& path)
{
    cro::ConfigFile cfg("treeset");
    cfg.addProperty("model").setValue(treeset.modelPath);
    cfg.addProperty("texture").setValue(treeset.texturePath);
    cfg.addProperty("colour").setValue(treeset.colour);
    cfg.addProperty("randomness").setValue(treeset.randomAmount);
    cfg.addProperty("leaf_size").setValue(treeset.leafSize);

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


    if (cfg.save(path))
    {
        lastTreeset = path;
    }
}

void BushState::loadSkyboxFile()
{
    auto path = cro::FileSystem::openFileDialogue("", "sbf");
    if (!path.empty())
    {
        auto ents = m_skyScene.getSystem<cro::ModelRenderer>()->getEntities();
        for (auto e : ents)
        {
            m_skyScene.destroyEntity(e);
        }

        loadSkybox(path, m_skyScene, m_resources, SkyboxMaterials());
        const auto& colours = m_skyScene.getSkyboxColours();
        skyMid = colours.middle;
        skyTop = colours.top;

        lastSkybox = cro::FileSystem::getFileName(path);
    }
}

void BushState::saveSkyboxFile()
{
    auto path = cro::FileSystem::saveFileDialogue("assets/golf/skyboxes/" + lastSkybox, "sbf");
    if (!path.empty())
    {
        cro::ConfigFile cfg;
        cfg.addProperty("sky_top").setValue(skyTop);
        cfg.addProperty("sky_bottom").setValue(skyMid);

        //TODO add path to cloud sprite sheet

        std::string modelPath = "assets/golf/models/skybox/";
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
                prop->addProperty("model").setValue(modelPath + e.getLabel());
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
    auto path = cro::FileSystem::openFileDialogue("", "cmt");
    if (!path.empty())
    {
        cro::ModelDefinition md(m_resources);
        if (md.loadFromFile(path))
        {
            auto entity = m_skyScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.setLabel(cro::FileSystem::getFileName(path));
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

    std::vector<std::string> inPaths;
    std::vector<std::string> outPaths;

    auto dirs = cro::FileSystem::listDirectories("assets/golf/courses");
    dirs.erase(std::remove_if(dirs.begin(), dirs.end(), [](const std::string& s) {return s.find("course_") == std::string::npos; }), dirs.end());

    for (auto dir : dirs)
    {
        auto path = "assets/golf/courses/" + dir + "/course.data";
        if (cro::FileSystem::fileExists(path))
        {
            inPaths.push_back(path);

            path = "assets/golf/thumbs/" + dir;
            if (!cro::FileSystem::directoryExists(path))
            {
                LogI << "creating directory " << dir << "..." << std::endl;
                cro::FileSystem::createDirectory(path);
            }
            outPaths.push_back(path);
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

    auto unlitShader = m_resources.shaders.loadBuiltIn(cro::ShaderResource::BuiltIn::Unlit, cro::ShaderResource::BuiltInFlags::DiffuseMap);
    auto matID = m_resources.materials.add(m_resources.shaders.get(unlitShader));
    auto material = m_resources.materials.get(matID);

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
                        entity.addComponent<cro::Transform>().setOrigin({160.f, 0.f, -100.f});
                        entity.getComponent<cro::Transform>().setPosition({160.f, 0.f, -100.f});
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
                        auto size = bounds[1] - bounds[0];
                        float scale = 1.f;
                        if (size.x > size.z)
                        {
                            scale = std::max(1.f, std::floor(320.f / size.x));
                        }
                        else
                        {
                            scale = std::max(1.f, std::floor(200.f / size.z));
                        }
                        entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));


                        glm::vec3 flagPos = glm::vec3(0.f);
                        auto* flag = holeFile.findProperty("pin");
                        if (flag)
                        {
                            flagPos = flag->getValue<glm::vec3>();

                            flagPos -= glm::vec3(160.f, 0.f, -100.f);
                            flagPos *= scale;
                            flagPos += glm::vec3(160.f, 0.f, -100.f);

                            flagQuad.setPosition({ flagPos.x / (320.f / ThumbnailSize.x), -flagPos.z / (200.f / ThumbnailSize.y)});
                        }



                        m_gameScene.simulate(0.f);

                        bufferTexture.clear(cro::Colour::Transparent);
                        m_gameScene.render();
                        bufferTexture.display();

                        m_thumbnailTexture.clear(/*cro::Colour(std::uint8_t(39), 56, 153)*/cro::Colour::Transparent);
                        bufferQuad.setPosition({ 2.f, -2.f });
                        bufferQuad.setColour(DropShadowColour); //constify this - same as minimap in GolfStateUI.cpp
                        bufferQuad.draw();
                        bufferQuad.setPosition({ 0.f, 0.f });
                        bufferQuad.setColour(cro::Colour::White);
                        bufferQuad.draw();
                        flagQuad.draw();
                        m_thumbnailTexture.display();

                        auto fileName = cro::FileSystem::getFileName(hole);
                        fileName = fileName.substr(0, fileName.find_last_of('.'));
                        m_thumbnailTexture.saveToFile(outPath + "/" + fileName + ".png");

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