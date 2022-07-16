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

#include "BushState.hpp"

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

namespace
{
#include "../golf/TreeShader.inl"

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
    const glm::uvec2 BillboardTargetSize(320, 448);
}

BushState::BushState(cro::StateStack& stack, cro::State::Context context)
    : cro::State        (stack, context),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_windBuffer        ("WindValues"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_scaleBuffer       ("PixelScale")
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
    context.appInstance.setClearColour(cro::Colour::CornflowerBlue);

    palette.loadFromFile("mod_kit/colordome-32.ase");
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
    m_uiScene.forwardEvent(evt);
    return false;
}

void BushState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
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
    m_uiScene.simulate(dt);
    return true;
}

void BushState::render()
{
    m_windBuffer.bind(0);
    m_resolutionBuffer.bind(1);
    m_scaleBuffer.bind(2);

    glEnable(GL_PROGRAM_POINT_SIZE);

    auto oldCam = m_gameScene.setActiveCamera(m_billboardCamera);
    m_billboardTexture.clear(cro::Colour::Transparent);
    m_gameScene.render();
    m_billboardTexture.display();

    m_gameScene.setActiveCamera(oldCam);
    m_backgroundQuad.draw();
    m_gameScene.render();
    m_uiScene.render();
}

//private
void BushState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb)->setNumCascades(1);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void BushState::loadAssets()
{
    m_resources.textures.setFallbackColour(cro::Colour::CornflowerBlue);
    auto& quadTex = m_resources.textures.get(std::numeric_limits<std::uint32_t>::max());
    m_backgroundQuad.setScale(cro::App::getWindow().getSize() / quadTex.getSize());
    m_backgroundQuad.setTexture(quadTex);

    m_scaleBuffer.setData(1.f);


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

    auto& material = m_resources.materials.get(bushMaterial);
    material.setProperty("u_diffuseMap", texture);

    shaderUniform.shaderID = shader->getGLHandle();
    shaderUniform.randomness = shader->getUniformID("u_randAmount");
    shaderUniform.size = shader->getUniformID("u_leafSize");
    shaderUniform.colour = shader->getUniformID("u_colour");
    shaderUniform.targetHeight = shader->getUniformID("u_targetHeight");


    m_resources.shaders.loadFromString(BushShaderID::Branch, BranchVertex, BranchFragment, "#define INSTANCING\n");
    shader = &m_resources.shaders.get(BushShaderID::Branch);
    branchMaterial = m_resources.materials.add(*shader);

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
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        float offset = entity.getComponent<cro::Model>().getMeshData().boundingBox[0].y;
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, offset, 0.f });

        m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    if (md.loadFromFile("assets/models/ground_plane.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }


    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&BushState::updateView, this, std::placeholders::_1);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(2048, 2048);
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 6.f });


    auto sun = m_gameScene.getSunlight();
    sun.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 35.f * cro::Util::Const::degToRad);
    sun.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -35.f * cro::Util::Const::degToRad);



    //ortho cam for billboard view
    glm::vec2 camSize = glm::vec2(BillboardTargetSize) / PixelsPerMetre;
    m_billboardCamera = m_gameScene.createEntity();
    m_billboardCamera.addComponent<cro::Transform>().setPosition({0.f, 0.f, 3.f});
    m_billboardCamera.addComponent<cro::Camera>().setOrthographic(-(camSize.x) / 2.f, camSize.x / 2.f, 0.f, camSize.y, 0.1f, 10.f);
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
    m_resolutionBuffer.setData(rd);

    //update the UI camera to match the new screen size
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;


    //resize the background quad
    m_backgroundQuad.setScale(cro::App::getWindow().getSize() / glm::uvec2(m_backgroundQuad.getSize()));
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
                }
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
                ImVec4 c(colour.getVec4());
                if (ImGui::ColorButton(std::to_string(i).c_str(), c))
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

            for (auto i = 0u; i < m_materials.size(); ++i)
            {
                auto label = "Material##" + std::to_string(i);
                if (ImGui::InputInt(label.c_str(), &m_materials[i].activeMaterial))
                {
                    m_materials[i].activeMaterial = std::min(1, std::max(0, m_materials[i].activeMaterial));

                    for (auto j = 1u; j < m_models.size(); ++j)
                    {
                        m_models[j].getComponent<cro::Model>().setMaterial(i, m_materials[i].materials[m_materials[i].activeMaterial]);
                        m_models[j].getComponent<cro::Model>().getMeshData().indexData[i].primitiveType =
                            m_materials[i].activeMaterial ? GL_POINTS : GL_TRIANGLES;
                    }
                }
            }
        }

    }
    ImGui::End();

    if (ImGui::Begin("Billboard"))
    {
        ImGui::Image(m_billboardTexture.getTexture(), {300.f, 400.f}, {0.f, 1.f}, {1.f, 0.f});

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
            
            m_root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            entity.getComponent<cro::Transform>().setPosition({ radius, 0.f, 0.f });


            auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

            //set branch as default
            for (auto j = 0u; j < meshData->submeshCount; ++j)
            {
                auto mat = m_resources.materials.get(branchMaterial);
                if (md.getMaterial(j)->properties.count("u_diffuseMap"))
                {
                    cro::TextureID tid(md.getMaterial(j)->properties.at("u_diffuseMap").second.textureID);
                    mat.setProperty("u_diffuseMap", tid);
                }
                entity.getComponent<cro::Model>().setMaterial(j, mat);

                m_materials[j].materials[0] = mat;
                m_materials[j].materials[1] = m_resources.materials.get(bushMaterial);
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
                treeset.randomAmount = std::max(treeset.randomAmount, std::min(1.f, p.getValue<float>()));
            }
            else if (name == "leaf_size")
            {
                treeset.leafSize = std::max(treeset.leafSize, std::min(1.f, p.getValue<float>()));
            }
            else if (name == "branch_index")
            {
                branchIndices.push_back(p.getValue<std::uint32_t>());

                auto matIndex = p.getValue<std::uint32_t>();
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
                m_models[j].getComponent<cro::Model>().setMaterial(i, m_materials[i].materials[m_materials[i].activeMaterial]);
                m_models[j].getComponent<cro::Model>().getMeshData().indexData[i].primitiveType =
                    m_materials[i].activeMaterial ? GL_POINTS : GL_TRIANGLES;
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