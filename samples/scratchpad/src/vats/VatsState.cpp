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

#include "VatsState.hpp"
#include "VatFile.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

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

    uniform mat4 u_worldMatrix;
    uniform mat3 u_normalMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform sampler2D u_positionMap;
    uniform sampler2D u_normalMap;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec2 v_texCoord;

    //const float UVOffset = (1.0 / 35.0) / 2.0;

    vec3 decodeVector(sampler2D source, vec2 coord)
    {
        vec3 vec = TEXTURE(source, coord).rgb;
        vec *= 2.0;
        vec -= 1.0;

        return vec;
    }

    void main()
    {
#if defined(NO_VATS)
        gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;
        v_normal = u_normalMatrix * a_normal;
#else
        vec2 texCoord = a_texCoord1;
        float scale = texCoord.y;
        texCoord.y = 0.0; //TODO add frame offset

        vec4 position = vec4(decodeVector(u_positionMap, texCoord) * scale, 1.0);
        gl_Position = u_viewProjectionMatrix * u_worldMatrix * position;
        v_normal = u_normalMatrix * normalize(decodeVector(u_normalMap, texCoord));
        //v_normal = u_normalMatrix * a_normal;
#endif
        v_colour = a_colour;
        v_texCoord = a_texCoord0;
    })";

    std::string Fragment = 
    R"(
    OUTPUT

    uniform vec3 u_lightDirection;

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    void main()
    {
        float amount = clamp(dot(normalize(v_normal), normalize(-u_lightDirection)), 0.1, 1.0);

        FRAG_OUT = vec4(1.0) * amount;
    })";
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

                if (m_model.isValid())
                {
                    static float rotation = 0.f;
                    if (ImGui::DragFloat("Rotation", &rotation, 1.f, 0.f, 360.f))
                    {
                        m_model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                        m_reference.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                        m_reference.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);
                    }

                    if (ImGui::Button("Create Texture"))
                    {
                        createNormalTexture();
                    }
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
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void VatsState::loadAssets()
{
    m_resources.shaders.loadFromString(ShaderID::NoVats, Vertex, Fragment, "#define NO_VATS\n");
    auto* shader = &m_resources.shaders.get(ShaderID::NoVats);
    m_materialIDs[MaterialID::NoVats] = m_resources.materials.add(*shader);
    
    m_resources.shaders.loadFromString(ShaderID::Vats, Vertex, Fragment);
    shader = &m_resources.shaders.get(ShaderID::Vats);
    m_materialIDs[MaterialID::Vats] = m_resources.materials.add(*shader);

    /*m_resources.shaders.loadFromString(ShaderID::VatsInstanced, "", "", "#define INSTANCED\n");
    auto* shader = &m_resources.shaders.get(ShaderID::VatsInstanced);
    m_materialIDs[MaterialID::VatsInstanced] = m_resources.materials.add(*shader);*/
}

void VatsState::createScene()
{
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
            m_model.addComponent<cro::Transform>().setPosition({ 0.6f, 0.f, 0.f});
            md.createModel(m_model);

            m_reference = m_gameScene.createEntity();
            m_reference.addComponent<cro::Transform>().setPosition({ -0.6f, 0.f, 0.f });
            m_reference.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);
            md.createModel(m_reference);

            m_reference.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::NoVats]));
        }

        m_positionTexture.loadFromFile(file.getPositionPath());
        m_normalTexture.loadFromFile(file.getNormalPath());

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Vats]);
        material.setProperty("u_positionMap", m_positionTexture);
        material.setProperty("u_normalMap", m_normalTexture);

        m_model.getComponent<cro::Model>().setMaterial(0, material);
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
            //the model already has this set so we can read it back to know
            //where the destination pixel is in the target
            auto vertexCount = 712;
            std::vector<std::uint8_t> imageBuffer(vertexCount * 3);
            const float pixelWidth = 1.f / vertexCount;
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



            for (auto i = 0; i < vertexCount; ++i)
            {
                auto vertIndex = i * stride;
                float pixelPos = verts[vertIndex + uv1Offset];
                pixelPos -= (pixelWidth / 2.f);

                std::int32_t pixelIndex = static_cast<std::int32_t>(pixelPos / pixelWidth) * 3;

                imageBuffer[pixelIndex] = static_cast<std::uint8_t>(unsign(verts[vertIndex + normalOffset]) * 255.f);
                imageBuffer[pixelIndex + 1] = static_cast<std::uint8_t>(unsign(verts[vertIndex + normalOffset + 1]) * 255.f);
                imageBuffer[pixelIndex + 2] = static_cast<std::uint8_t>(unsign(verts[vertIndex + normalOffset + 2]) * 255.f);
            }

            cro::Image img;
            img.loadFromMemory(imageBuffer.data(), vertexCount, 1, cro::ImageFormat::RGB);
            img.write("assets/vats/normals.png");

            //glBindTexture(GL_TEXTURE_2D, m_normalTexture.getGLHandle());
            //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, vertexCount, 1, 0, GL_FLOAT, GL_RGB, imageBuffer.data());
            //glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
}