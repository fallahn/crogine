/*-----------------------------------------------------------------------

Matt Marchant 2023
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

//Auto-generated source file for Scratchpad Stub 05/01/2023, 21:01:55

#include "AnimBlendState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/detail/OpenGL.hpp>

namespace
{
    //shell shaders
    const std::string ShellVert = 
        R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_colour;
ATTRIBUTE vec2 a_texCoord0;

uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;
uniform vec3 u_cameraWorldPosition;

VARYING_OUT vec2 v_texCoord;
VARYING_OUT vec2 v_height;
VARYING_OUT float v_viewDistance;

void main()
{
    vec4 worldPos = u_worldMatrix * a_position;
    v_viewDistance = length(worldPos.xyz - u_cameraWorldPosition);

    gl_Position = u_viewProjectionMatrix * worldPos;
    v_texCoord = a_texCoord0;
    v_height = a_colour.rg;
}
        )";


    const std::string ShellFrag = 
        R"(
OUTPUT
uniform sampler2D u_shellTexture;
uniform sampler2D u_noiseTexture;
uniform float u_time;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec2 v_height;
VARYING_IN float v_viewDistance;

//TODO replace with uniform
const vec4 BladeColour = vec4(0.275,0.494,0.243,1.0);
const vec4 ShadowColour = vec4(0.157,0.306,0.263, 1.0);
const vec4 GroundColour = vec4(0.314,0.157,0.184,1.0);

const vec2 WindDir = vec2(0.02);

void main()
{
    float distance = 1.0 - smoothstep(0.1, 0.9, v_viewDistance / 5.0);

    float height = v_height.r;
    float noiseHeight = TEXTURE(u_noiseTexture, v_texCoord / 2.0).r;

    float windAmount = TEXTURE(u_noiseTexture, (v_texCoord / 4.0) + (u_time * 0.02)).r * height;

    vec4 colour = mix(ShadowColour * 0.5, BladeColour, height);
    colour = mix(ShadowColour * height, colour, ((0.9 * noiseHeight) + 0.1));

    vec4 shellSample = TEXTURE(u_shellTexture, (WindDir * windAmount) + v_texCoord) * ((0.5 * noiseHeight) + 0.5);

    float visibility = (1.0 - step(shellSample.r, height)) * shellSample.a;
    colour.a = visibility;

    //TODO if we're on top of other geom we don't need a ground colour
    colour = mix(GroundColour * distance, colour, step(v_height.g, v_height.r));

    FRAG_OUT = colour;
}
        )";

    struct ShaderID final
    {
        enum
        {
            Shell
        };
    };
    std::int32_t timeUniform = -1;
    std::int32_t shaderHandle = 0;

    struct MaterialID final
    {
        enum
        {
            Shell
        };
    };

    //league testing
    std::size_t courseIndex = 0;
    const std::vector<std::array<std::int32_t, 18>> courseData =
    {
        std::array<std::int32_t, 18>{4,4,3,3,3,4,4,3,2,3,4,4,5,3,2,3,4,3},
        std::array<std::int32_t, 18>{4,4,4,3,2,3,4,5,3,3,4,3,4,3,2,3,4,3},
        std::array<std::int32_t, 18>{4,3,2,3,3,2,4,3,3,4,3,3,4,4,4,5,4,3},
        std::array<std::int32_t, 18>{4,3,2,3,4,4,3,2,4,3,3,4,4,3,5,3,4,3}
    };

    const std::vector<std::uint8_t> testScores =
    {
        3,3,2,2,2,3,2,3,1,2,2,3,3,2,3,2,2,3
    };
}

AnimBlendState::AnimBlendState(cro::StateStack& stack, cro::State::Context context)
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
bool AnimBlendState::handleEvent(const cro::Event& evt)
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
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;

        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void AnimBlendState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool AnimBlendState::simulate(float dt)
{
    static float accum = 0.f;
    accum += dt;
    glUseProgram(shaderHandle);
    glUniform1f(timeUniform, accum);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void AnimBlendState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void AnimBlendState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void AnimBlendState::loadAssets()
{
    //generate a shell texture
    static constexpr std::uint32_t width = 512;
    static constexpr std::uint32_t height = 512;

    static constexpr std::int32_t LayerCount = 24;
    static constexpr float Density = 0.6f;

    static constexpr std::int32_t StrandCount = static_cast<float>(width * height) * Density;
    static constexpr std::int32_t StrandsPerLayer = StrandCount / LayerCount;

    std::vector<std::uint8_t> imgArray(width * height * 4);
    std::fill(imgArray.begin(), imgArray.end(), 0);

    for (auto i = 0; i < StrandCount; ++i)
    {
        const auto x = cro::Util::Random::value(0, width - 1);
        const auto y = cro::Util::Random::value(0, height - 1);
        const float MaxLayer = std::pow(static_cast<float>(i / StrandsPerLayer) / LayerCount, 0.7f);

        const auto idx = (x * width + y) * 4;
        imgArray[idx] = static_cast<std::uint8_t>(MaxLayer * 255.f);
        imgArray[idx+3] = 255;
    }

    m_shellTexture.create(width, height);
    m_shellTexture.update(imgArray.data());
    m_shellTexture.setSmooth(true);

    m_noiseTexture.loadFromFile("assets/images/noise.png");
    m_noiseTexture.setRepeated(true);
    m_noiseTexture.setSmooth(true);

    //shell material
    m_resources.shaders.loadFromString(ShaderID::Shell, ShellVert, ShellFrag);
    auto& shader = m_resources.shaders.get(ShaderID::Shell);
    shaderHandle = shader.getGLHandle();
    timeUniform = shader.getUniformID("u_time");

    m_resources.materials.add(MaterialID::Shell, shader);
    m_resources.materials.get(MaterialID::Shell).setProperty("u_shellTexture", m_shellTexture);
    m_resources.materials.get(MaterialID::Shell).setProperty("u_noiseTexture", m_noiseTexture);
    m_resources.materials.get(MaterialID::Shell).blendMode = cro::Material::BlendMode::Alpha;


    //shell mesh
    static constexpr std::array<glm::vec3, 4u> Corners =
    {
        glm::vec3(-1.f, 0.f, -1.f),
        glm::vec3(-1.f, 0.f, 1.f),
        glm::vec3(1.f, 0.f, -1.f),
        glm::vec3(1.f, 0.f, 1.f)
    };

    static constexpr std::array<glm::vec2, 4u> UVs =
    {
        glm::vec2(0.f, 1.f),
        glm::vec2(0.f, 0.f),
        glm::vec2(1.f, 1.f),
        glm::vec2(1.f, 0.f),
    };

    struct Vertex final
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec4 colour = glm::vec4(1.f);
        glm::vec2 uv = glm::vec2(0.f);
        Vertex(glm::vec3 p, glm::vec4 c, glm::vec2 u)
            : position(p), colour(c), uv(u) {}
    };

    float y = 0.f;
    static constexpr float MaxHeight = 0.1f;
    static constexpr float LayerHeight = MaxHeight / LayerCount;

    std::vector<Vertex> vertData;
    std::vector<std::uint32_t> indexData;

    for (auto i = 0; i < LayerCount; ++i)
    {
        glm::vec4 colour = glm::vec4(((i + 1) * LayerHeight) / MaxHeight, (LayerHeight / MaxHeight) - 0.001f, 0.f, 1.f);

        for (auto j = 0; j < 4; ++j)
        {
            auto p = Corners[j];
            p.y = y;

            vertData.emplace_back(p, colour, UVs[j]);
        }

        const std::uint32_t Offset = static_cast<std::uint32_t>(vertData.size());
        indexData.push_back(Offset);
        indexData.push_back(Offset + 1);
        indexData.push_back(Offset + 2);

        indexData.push_back(Offset + 2);
        indexData.push_back(Offset + 1);
        indexData.push_back(Offset + 3);

        y += LayerHeight;
    }




    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::UV0, 1, GL_TRIANGLES));
    auto material = m_resources.materials.get(MaterialID::Shell);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

    meshData->boundingBox = { glm::vec3(-1.f), glm::vec3(1.f) };
    meshData->boundingSphere = meshData->boundingBox;

    auto vertStride = (meshData->vertexSize / sizeof(float));
    meshData->vertexCount = static_cast<std::uint32_t>(vertData.size());
    (glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    (glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, vertData.data(), GL_STATIC_DRAW));
    (glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indexData.size());
    (glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    (glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indexData.data(), GL_STATIC_DRAW));
    (glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

void AnimBlendState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/01.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.2f);
        md.createModel(entity);

        if (entity.hasComponent<cro::Skeleton>())
        {
            auto animCount = entity.getComponent<cro::Skeleton>().getAnimations().size();

            //this makes some suppositions about the model, but we're
            //here for a reason...
            if (animCount > 2)
            {
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(2.f);
                entity.getComponent<cro::Callback>().function =
                    [&, animCount](cro::Entity e, float dt)
                {
                    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                    currTime -= dt * m_gameScene.getSystem<cro::SkeletalAnimator>()->getPlaybackRate();

                    if (currTime < 0)
                    {
                        e.getComponent<cro::Skeleton>().play(cro::Util::Random::value(0, 1) ? 0 : (animCount - 1), 1.f, 0.5f);
                        currTime = static_cast<float>(cro::Util::Random::value(8, 16));
                    }

                    if (e.getComponent<cro::Skeleton>().getState() == cro::Skeleton::Stopped)
                    {
                        e.getComponent<cro::Skeleton>().play(1, 1.f, 0.5f);
                    }
                };
            }
            entity.getComponent<cro::Skeleton>().play(1, 1.f);
        }

        m_modelEntity = entity;


        const std::array pos =
        {
            glm::vec3(1.f, -0.05f, 0.f),
            glm::vec3(1.f, -0.05f, -1.f),
            glm::vec3(-1.f, -0.05f, -1.f),
        };

        for (auto i = 0u; i < 3u; ++i)
        {
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos[i]);
            entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, cro::Util:: Const::PI + (i * (cro::Util::Const::PI / 2.f)));
            md.createModel(entity);
            if (entity.hasComponent<cro::Skeleton>())
            {
                entity.getComponent<cro::Skeleton>().play(1);
                entity.addComponent<cro::Callback>() = m_modelEntity.getComponent<cro::Callback>();
            }
        }
    }


    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 1.f, 0.f });
    entity.addComponent<cro::ParticleEmitter>().settings.loadFromFile("assets/particles/dropsy.cps", m_resources.textures);
    entity.getComponent<cro::ParticleEmitter>().start();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        static const auto WaveTable = cro::Util::Wavetable::sine(1.f);
        static std::size_t s = 0;
        static std::size_t c = WaveTable.size() / 4;

        glm::vec3 pos(WaveTable[c], 1.f, WaveTable[s]);
        e.getComponent<cro::Transform>().setPosition(pos);

        s = (s + 1) % WaveTable.size();
        c = (c + 1) % WaveTable.size();

        /*auto pos = e.getComponent<cro::Transform>().getPosition();
        pos.x += dt * 6.f;
        if (pos.x > 2.f)
        {
            pos.x -= 4.f;
        }*/
        e.getComponent<cro::Transform>().setPosition(pos);
    };

    registerWindow([entity]() mutable
        {
            if (ImGui::Begin("P"))
            {
                float rate = entity.getComponent<cro::ParticleEmitter>().settings.emitRate;
                if (ImGui::SliderFloat("Rate", &rate, 10.f, 300.f))
                {
                    entity.getComponent<cro::ParticleEmitter>().settings.emitRate = rate;
                }
            }
            ImGui::End();
        
        });


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(65.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.6f, 2.f });
}

void AnimBlendState::createUI()
{
    registerWindow([&]()
        {
            if (ImGui::Begin("Buns"))
            {
                ImGui::Image(m_shellTexture, { 512.f, 512.f }, { 0.f, 1.f }, { 1.f, 0.f });
            }
            ImGui::End();

            /*if (ImGui::Begin("League"))
            {
                const auto& entries = m_league.getTable();
                for (const auto& e : entries)
                {
                    ImGui::Text("Skill: %d - Curve: %d - Score: %d - Name: %d", e.skill, e.curve, e.currentScore, e.nameIndex);
                }
                ImGui::Text("Iteratation: %d", m_league.getCurrentIteration());
                ImGui::SameLine();
                ImGui::Text("Season: %d", m_league.getCurrentSeason());
                ImGui::SameLine();
                ImGui::Text("Score: %d", m_league.getCurrentScore());

                if (ImGui::Button("Iterate"))
                {
                    m_league.iterate(courseData[courseIndex], testScores, 0);
                    courseIndex = (courseIndex + 1) % courseData.size();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    m_league.reset();
                }

                if (ImGui::Button("Run 10 Seasons"))
                {
                    for (auto i = 0; i < 10; ++i)
                    {
                        for (auto j = 0; j < League::MaxIterations; ++j)
                        {
                            m_league.iterate(courseData[courseIndex], testScores, cro::Util::Random::value(0,2));
                            courseIndex = (courseIndex + 1) % courseData.size();
                        }
                    }
                }
            }
            ImGui::End();*/

            if (m_modelEntity.isValid())
            {
                if (ImGui::Begin("Controls"))
                {
                    if (m_modelEntity.hasComponent<cro::Skeleton>())
                    {
                        static bool step = false;
                        if (step)
                        {
                            if (ImGui::Button("Play Mode"))
                            {
                                step = false;
                                m_gameScene.setSystemActive<cro::SkeletalAnimator>(true);
                                m_modelEntity.getComponent<cro::Callback>().active = true;
                                m_modelEntity.getComponent<cro::Skeleton>().play(0, 1.f, 0.5f);
                            }

                            ImGui::Separator();
                            static float stepRate = 1.f / 60.f;
                            ImGui::SliderFloat("Step Rate", &stepRate, 1.f / 60.f, 0.2f);
                            if (ImGui::Button("Step"))
                            {
                                m_gameScene.getSystem<cro::SkeletalAnimator>()->process(stepRate);
                            }
                        }
                        else
                        {
                            if (ImGui::Button("Step Mode"))
                            {
                                step = true;
                                m_gameScene.setSystemActive<cro::SkeletalAnimator>(false);
                                m_modelEntity.getComponent<cro::Callback>().active = false;
                            }
                        }

                        ImGui::NewLine();
                        bool interp = m_modelEntity.getComponent<cro::Skeleton>().getInterpolationEnabled();
                        if (ImGui::Checkbox("Interpolation", &interp))
                        {
                            m_modelEntity.getComponent<cro::Skeleton>().setInterpolationEnabled(interp);
                        }
                        m_gameScene.getSystem<cro::SkeletalAnimator>()->debugUI();
                    }
                    else
                    {
                        ImGui::Text("No Skeleton In Model");
                    }
                }
                ImGui::End();
            }
        });

    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}