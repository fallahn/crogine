/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "VatsState.hpp"
#include "VatFile.hpp"

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>

namespace
{
    const std::string Vertex =
    R"(
    
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec2 a_texCoord0;
    ATTRIBUTE vec2 a_texCoord1;

#if defined(INSTANCING)
    ATTRIBUTE mat4 a_instanceWorldMatrix;
    ATTRIBUTE mat3 a_instanceNormalMatrix;
#endif

#if defined(INSTANCING)
    uniform mat4 u_viewMatrix;
#else
    uniform mat4 u_worldViewMatrix;
    uniform mat3 u_normalMatrix;
#endif
    uniform mat4 u_worldMatrix;
    uniform mat4 u_projectionMatrix;

#if defined(ARRAY_MAPPING)
    uniform sampler2DArray u_arrayMap;
#else
    uniform sampler2D u_positionMap;
    uniform sampler2D u_normalMap;
#endif
    uniform float u_time;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec2 v_texCoord;
    flat VARYING_OUT int v_instanceID;

#if defined(ARRAY_MAPPING)
    vec3 decodeVector(sampler2DArray source, vec3 coord)
    {
        vec3 vec = texture(source, coord).rgb;
        vec *= 2.0;
        vec -= 1.0;

        return vec;
    }
#else
    vec3 decodeVector(sampler2D source, vec2 coord)
    {
        vec3 vec = TEXTURE(source, coord).rgb;
        vec *= 2.0;
        vec -= 1.0;

        return vec;
    }
#endif

    const float AnimationOffset = 0.34;    

    void main()
    {
#if defined (INSTANCING)
        mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
        mat4 worldViewMatrix = u_viewMatrix * worldMatrix;
        mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;
        v_instanceID = gl_InstanceID;
#else
        mat4 worldMatrix = u_worldMatrix;
        mat4 worldViewMatrix = u_worldViewMatrix;
        mat3 normalMatrix = u_normalMatrix;
#endif


#if defined(NO_VATS)
        gl_Position = u_projectionMatrix * worldViewMatrix * a_position;
        v_normal = normalMatrix * a_normal;
#else

#if defined(ARRAY_MAPPING)
        vec3 texCoord = vec3(a_texCoord1, 1.0); //1 is position

        float scale = texCoord.y;
        texCoord.y = mod((u_time * 0.1) + (gl_InstanceID * AnimationOffset), 1.0);

        vec4 position = vec4(decodeVector(u_arrayMap, texCoord) * scale, 1.0);
        gl_Position = u_projectionMatrix * worldViewMatrix * position;

        texCoord.z = 2.0;
        v_normal = normalMatrix * (decodeVector(u_arrayMap, texCoord));
#else
        vec2 texCoord = a_texCoord1;
        float scale = texCoord.y;
        texCoord.y = mod(u_time + (gl_InstanceID * AnimationOffset), 1.0);

        vec4 position = vec4(decodeVector(u_positionMap, texCoord) * scale, 1.0);
        gl_Position = u_projectionMatrix * worldViewMatrix * position;
        v_normal = normalMatrix * (decodeVector(u_normalMap, texCoord));
#endif
#endif
        v_colour = a_colour;
        v_texCoord = a_texCoord0;
    })";

    std::string Fragment = 
    R"(
    OUTPUT

    uniform vec3 u_lightDirection;

#if defined (ARRAY_MAPPING)
    uniform sampler2DArray u_arrayMap;
#endif

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;
    flat VARYING_IN int v_instanceID;

    const vec4[3] Colours = vec4[3]
    (
        vec4(1.0, 0.0, 0.0, 1.0),
        vec4(0.0, 1.0, 0.0, 1.0),
        vec4(0.0, 0.0, 1.0, 1.0)
    );

    void main()
    {
        float amount = clamp(dot(normalize(v_normal), normalize(-u_lightDirection)), 0.4, 1.0);
#if defined(ARRAY_MAPPING)
        FRAG_OUT = texture(u_arrayMap, vec3(v_texCoord, 0.0));// * amount;
#else
        FRAG_OUT = Colours[v_instanceID] * amount;
#endif

    })";

    std::int32_t timeUniform = -1;
    std::uint32_t shaderID = 0;
}

VatsState::VatsState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
    });

    registerWindow([&]() 
        {
            if (ImGui::Begin("Buns"))
            {
                if (ImGui::Button("Open File"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "vat");
                    if (!path.empty())
                    {
                        loadModel(path);
                    }
                }

                ImGui::SameLine();
                static float stars = 0.f;
                if (ImGui::SliderFloat("Stars", &stars, 0.f, 1.f))
                {
                    m_gameScene.setStarsAmount(stars);
                }

                if (m_model.isValid())
                {
                    static float rotation = 0.f;
                    if (ImGui::DragFloat("Rotation", &rotation, 1.f, 0.f, 360.f))
                    {
                        m_model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                        m_reference.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                        m_reference.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);
                    }

                    /*if (ImGui::Button("Create Texture"))
                    {
                        createNormalTexture();
                    }*/
                }
            }
            ImGui::End();
        });

    cro::App::getInstance().setClearColour(cro::Colour::CornflowerBlue);
}

//public
bool VatsState::handleEvent(const cro::Event& evt)
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
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    return true;
}

void VatsState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
}

bool VatsState::simulate(float dt)
{
    static float timeAccum = 0.f;
    timeAccum += dt;

    glUseProgram(shaderID);
    glUniform1f(timeUniform, timeAccum);
    glUseProgram(0);

    m_gameScene.simulate(dt);
    return true;
}

void VatsState::render()
{
    m_gameScene.render();
}

//private
void VatsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void VatsState::loadAssets()
{
    m_resources.shaders.loadFromString(ShaderID::NoVats, Vertex, Fragment, "#define NO_VATS\n");
    auto* shader = &m_resources.shaders.get(ShaderID::NoVats);
    m_materialIDs[MaterialID::NoVats] = m_resources.materials.add(*shader);
    
    m_resources.shaders.loadFromString(ShaderID::Vats, Vertex, Fragment, "#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::Vats);
    m_materialIDs[MaterialID::Vats] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::VatsArray, Vertex, Fragment, "#define INSTANCING\n#define ARRAY_MAPPING\n");
    shader = &m_resources.shaders.get(ShaderID::VatsArray);
    m_materialIDs[MaterialID::VatsArray] = m_resources.materials.add(*shader);

    timeUniform = shader->getUniformID("u_time");
    shaderID = shader->getGLHandle();

    /*m_resources.shaders.loadFromString(ShaderID::VatsInstanced, Vertex, Fragment, "#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::VatsInstanced);
    m_materialIDs[MaterialID::VatsInstanced] = m_resources.materials.add(*shader);*/
}

void VatsState::createScene()
{
    m_gameScene.enableSkybox();
    m_gameScene.setSkyboxColours(cro::Colour(0.921f,0.513f,0.054f), cro::Colour(0.176f,0.239f,0.321f), cro::Colour(0.004f,0.035f,0.105f));

    auto updateView = [&](cro::Camera& cam)
    {
        auto windowSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective(60.f * cro::Util::Const::degToRad, windowSize.x / windowSize.y, 0.1f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 1.6f, 2.2f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -15.f * cro::Util::Const::degToRad);

    //camEnt.addComponent<cro::Callback>().active = true;
    //camEnt.getComponent<cro::Callback>().function =
    //    [](cro::Entity e, float dt)
    //{
    //    e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
    //};

    auto sunlight = m_gameScene.getSunlight();
    sunlight.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -60.f * cro::Util::Const::degToRad);
}

void VatsState::loadModel(const std::string& path)
{
    if (m_model.isValid())
    {
        m_gameScene.destroyEntity(m_model);
    }

    if (m_reference.isValid())
    {
        m_gameScene.destroyEntity(m_reference);
    }

    VatFile file;
    if (file.loadFromFile(path))
    {
        cro::ModelDefinition md(m_resources);
        if (md.loadFromFile(file.getModelPath()))
        {
            m_model = m_gameScene.createEntity();
            m_model.addComponent<cro::Transform>().setPosition({ 0.45f, 0.f, 0.f});
            md.createModel(m_model);

            std::vector<glm::mat4> tx = { glm::mat4(1.f), glm::mat4(1.f), glm::mat4(1.f) };
            tx[1] = glm::translate(tx[1], glm::vec3(0.55f, 0.f, 0.1f));
            tx[2] = glm::translate(tx[1], glm::vec3(-1.2f, 0.f, 0.13f));
            m_model.getComponent<cro::Model>().setInstanceTransforms(tx);

            m_reference = m_gameScene.createEntity();
            m_reference.addComponent<cro::Transform>().setPosition({ -1.2f, 0.f, 0.f });
            m_reference.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);
            md.createModel(m_reference);

            m_reference.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::NoVats]));
        }

        if (file.fillArrayTexture(m_arrayTexture))
        {
            auto material = m_resources.materials.get(m_materialIDs[MaterialID::VatsArray]);
            material.setProperty("u_arrayMap", m_arrayTexture);

            m_model.getComponent<cro::Model>().setMaterial(0, material);

            auto* shader = &m_resources.shaders.get(ShaderID::VatsArray);
            timeUniform = shader->getUniformID("u_time");
            shaderID = shader->getGLHandle();
        }
        else
        {
            m_positionTexture.loadFromFile(file.getPositionPath());
            m_positionTexture.setSmooth(false);
            m_normalTexture.loadFromFile(file.getNormalPath());
            m_normalTexture.setSmooth(true);

            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Vats]);
            material.setProperty("u_positionMap", m_positionTexture);
            material.setProperty("u_normalMap", m_normalTexture);

            m_model.getComponent<cro::Model>().setMaterial(0, material);

            auto* shader = &m_resources.shaders.get(ShaderID::Vats);
            timeUniform = shader->getUniformID("u_time");
            shaderID = shader->getGLHandle();
        }
    }
}

void VatsState::createNormalTexture()
{
    if (m_reference.isValid())
    {
        const auto& meshData = m_reference.getComponent<cro::Model>().getMeshData();

        std::vector<float> verts;
        std::vector<std::vector<std::uint32_t>> indices;
        cro::Mesh::readVertexData(meshData, verts, indices);

        if (meshData.attributes[cro::Mesh::Attribute::UV1] != 0)
        {
            std::vector<std::uint8_t> positionBuffer(meshData.vertexCount * 3);
            std::vector<std::uint8_t> normalBuffer(meshData.vertexCount * 3);
            const auto stride = meshData.vertexSize / sizeof(float);

            std::size_t normalOffset = 0;
            for (auto i = 0u; i < cro::Mesh::Attribute::Normal; ++i)
            {
                normalOffset += meshData.attributes[i];
            }

            std::size_t uv1Offset = 0;
            for (auto i = 0u; i < cro::Mesh::Attribute::UV1; ++i)
            {
                uv1Offset += meshData.attributes[i];
            }

            const auto unsign = [](float v)
            {
                return (v + 1.f) / 2.f;
            };

            const float pixelWidth = 1.f / meshData.vertexCount;
            const float scale = 1.8f;

            for (auto i = 0; i < meshData.vertexCount; ++i)
            {
                auto vertIndex = i * stride;
                verts[vertIndex + uv1Offset] = (i * pixelWidth) + (pixelWidth / 2.f);
                verts[vertIndex + uv1Offset + 1] = scale;

                auto pixelIndex = i * 3;

                positionBuffer[pixelIndex] = static_cast<std::uint8_t>(unsign(verts[vertIndex] / scale) * 255.f);
                positionBuffer[pixelIndex + 1] = -static_cast<std::uint8_t>(unsign(verts[vertIndex + 2] / scale) * 255.f);
                positionBuffer[pixelIndex + 2] = static_cast<std::uint8_t>(unsign(verts[vertIndex + 1] / scale) * 255.f);

                normalBuffer[pixelIndex] = static_cast<std::uint8_t>(unsign(verts[vertIndex + normalOffset]) * 255.f);
                normalBuffer[pixelIndex + 1] = -static_cast<std::uint8_t>(unsign(verts[vertIndex + normalOffset + 2]) * 255.f);
                normalBuffer[pixelIndex + 2] = static_cast<std::uint8_t>(unsign(verts[vertIndex + normalOffset + 1]) * 255.f);
            }

            //we could bypass creating the image and load straight
            //to the texturem but we want to make use of the image write function

            cro::Image img;
            img.loadFromMemory(normalBuffer.data(), meshData.vertexCount, 1, cro::ImageFormat::RGB);
            //img.write("assets/vats/normals.png");
            m_normalTexture.loadFromImage(img);

            img.loadFromMemory(positionBuffer.data(), meshData.vertexCount, 1, cro::ImageFormat::RGB);
            m_positionTexture.loadFromImage(img);

            glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}