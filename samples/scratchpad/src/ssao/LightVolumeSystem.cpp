/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

crogine - Zlib license.

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

#include "LightVolumeSystem.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Message.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/Spatial.hpp>
#include <crogine/detail/OpenGL.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/Gui.hpp>
#endif

using namespace test;

namespace
{
    const std::string VertexShader = 
        R"(
        ATTRIBUTE vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;
        })";

    const std::string FragmentShader = 
        R"(
        OUTPUT

        uniform sampler2D u_normalMap;
        uniform sampler2D u_positionMap;

        uniform float u_lightRadiusSqr;
        uniform vec3 u_lightColour = vec3(1.0, 1.0, 0.0);
        uniform vec3 u_lightViewPos; //positions are in view space

        uniform vec2 u_targetSize = vec2(640.0, 480.0);

        void main()
        {
            vec2 texCoord = gl_FragCoord.xy / u_targetSize;
            vec3 normal = normalize(TEXTURE(u_normalMap, texCoord).rgb);
            vec3 position = TEXTURE(u_positionMap, texCoord).rgb;

            vec3 lightDir = u_lightViewPos - position;
            vec3 lightColour = u_lightColour * max(dot(normal, normalize(lightDir)), 0.0);

            //not perfectly accurate compared to the linear/quadratic equation but easier to involve the radius 
            float attenuation = 1.0 - min(dot(lightDir, lightDir) / u_lightRadiusSqr, 1.0);

            FRAG_OUT = vec4(lightColour * attenuation, 1.0);
        })";
}

LightVolumeSystem::LightVolumeSystem(cro::MessageBus& mb)
    : System        (mb, typeid(LightVolumeSystem)),
    m_bufferSize    (cro::App::getWindow().getSize()),
    m_bufferScale   (1),
    m_multiSamples  (0)
{
    requireComponent<LightVolume>();
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();

    std::fill(m_uniformIDs.begin(), m_uniformIDs.end(), -1);
    if (m_shader.loadFromString(VertexShader, FragmentShader))
    {
        m_uniformIDs[UniformID::World] = m_shader.getUniformID("u_worldMatrix");
        m_uniformIDs[UniformID::ViewProjection] = m_shader.getUniformID("u_viewProjectionMatrix");

        m_uniformIDs[UniformID::NormalMap] = m_shader.getUniformID("u_normalMap");
        m_uniformIDs[UniformID::PositionMap] = m_shader.getUniformID("u_positionMap");

        m_uniformIDs[UniformID::TargetSize] = m_shader.getUniformID("u_targetSize");

        m_uniformIDs[UniformID::LightColour] = m_shader.getUniformID("u_lightColour");
        m_uniformIDs[UniformID::LightPosition] = m_shader.getUniformID("u_lightViewPos");
        m_uniformIDs[UniformID::LightRadiusSqr] = m_shader.getUniformID("u_lightRadiusSqr");
    }

#ifdef CRO_DEBUG_
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Volume Lights"))
    //        {
    //            ImGui::Text("Visible Lights %u", m_visibleEntities.size());

    //            glm::vec2 size(m_bufferSize);
    //            ImGui::Image(m_renderTexture.getTexture(), { size.x / 4.f, size.y / 4.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //        }
    //        ImGui::End();
    //    });
#endif

    //always do this last
    resizeBuffer();
}

//public
void LightVolumeSystem::handleMessage(const cro::Message& msg)
{
    //resize buffer based on scale and target size
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_RESIZED)
        {
            //hmmmmm this is surely moot if we're not using the window size directly?
        }
    }
}

void LightVolumeSystem::process(float) {}

void LightVolumeSystem::updateDrawList(cro::Entity cameraEnt)
{
    m_visibleEntities.clear();
    const auto& camComponent = cameraEnt.getComponent<cro::Camera>();
    const auto& frustum = camComponent.getPass(cro::Camera::Pass::Final).getFrustum();
    const auto cameraPos = cameraEnt.getComponent<cro::Transform>().getWorldPosition();
    const auto& entities = getEntities();

    //TODO this only does lighting on the output pass, not the reflection
    //though this is probably enough for our case
    for (auto entity : entities)
    {
        const auto& model = entity.getComponent<cro::Model>();
        auto sphere = model.getBoundingSphere();
        const auto& tx = entity.getComponent<cro::Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getScale();
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

        const auto direction = (sphere.centre - cameraPos);
        const float distance = glm::dot(camComponent.getPass(cro::Camera::Pass::Final).forwardVector, direction);

        if (distance < -sphere.radius)
        {
            //model is behind the camera
            continue;
        }

        bool visible = true;
        std::size_t j = 0;
        while (visible && j < frustum.size())
        {
            visible = (cro::Spatial::intersects(frustum[j++], sphere) != cro::Planar::Back);
        }

        if (visible)
        {
            m_visibleEntities.push_back(entity);
        }
    }
}

void LightVolumeSystem::updateBuffer(cro::Entity camera)
{
    const auto& camComponent = camera.getComponent<cro::Camera>();
    const auto& pass = camComponent.getPass(cro::Camera::Pass::Final);
    const auto& camTx = camera.getComponent<cro::Transform>();
    const auto cameraPosition = camTx.getWorldPosition();

    /*glCheck*/(glFrontFace(GL_CCW)); //TODO enable glCheck
    /*glCheck*/(glEnable(GL_CULL_FACE));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_bufferIDs[BufferID::Normal].textureID);

    glActiveTexture(GL_TEXTURE0+1);
    glBindTexture(GL_TEXTURE_2D, m_bufferIDs[BufferID::Position].textureID);

    const auto viewProj = camComponent.getProjectionMatrix() * pass.viewMatrix;

    m_renderTexture.clear();
    /*glCheck*/(glUseProgram(m_shader.getGLHandle()));
    /*glCheck*/(glUniform1i(m_uniformIDs[UniformID::NormalMap], 0));
    /*glCheck*/(glUniform1i(m_uniformIDs[UniformID::PositionMap], 1));
    /*glCheck*/(glUniformMatrix4fv(m_uniformIDs[UniformID::ViewProjection], 1, GL_FALSE, &viewProj[0][0]));

    //if there are multiple lights blend additively
    //TODO we probably want to sort back to front when culling
    /*glCheck*/(glEnable(GL_BLEND));
    /*glCheck*/(glDisable(GL_DEPTH_TEST));
    /*glCheck*/(glBlendFunc(GL_ONE, GL_ONE));
    /*glCheck*/(glBlendEquation(GL_FUNC_ADD));


    for (const auto& entity : m_visibleEntities)
    {
        /*glCheck*/(glCullFace(GL_FRONT));

        const auto& tx = entity.getComponent<cro::Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        /*glCheck*/(glUniformMatrix4fv(m_uniformIDs[UniformID::World], 1, GL_FALSE, &worldMat[0][0]));

        const auto viewPos = glm::vec3(pass.viewMatrix * glm::vec4(tx.getWorldPosition(), 1.f));
        /*glCheck*/(glUniform3f(m_uniformIDs[UniformID::LightPosition], viewPos.x, viewPos.y, viewPos.z));

        const auto& light = entity.getComponent<LightVolume>();
        /*glCheck*/(glUniform3f(m_uniformIDs[UniformID::LightColour], light.colour.getRed(), light.colour.getGreen(), light.colour.getBlue()));
        /*glCheck*/(glUniform1f(m_uniformIDs[UniformID::LightRadiusSqr], light.radius * light.radius));

        entity.getComponent<cro::Model>().draw(0, cro::Mesh::IndexData::Final);
    }
    m_renderTexture.display();

    /*glCheck*/(glDisable(GL_CULL_FACE));
    /*glCheck*/(glDisable(GL_DEPTH_TEST));
    /*glCheck*/(glDepthMask(GL_TRUE));
}

void LightVolumeSystem::setSourceBuffer(cro::TextureID id, std::int32_t index)
{
    CRO_ASSERT(index != -1 && index < BufferID::Count, "");
    m_bufferIDs[index] = id;
}

void LightVolumeSystem::setTargetSize(glm::uvec2 size, std::uint32_t scale)
{
    bool wantsUpdate = size != m_bufferSize || scale != m_bufferScale;
    m_bufferSize = size;
    m_bufferScale = scale;
    CRO_ASSERT(m_bufferSize.x > 0 && m_bufferSize.y > 0 && m_bufferScale > 0, "");

    if (wantsUpdate)
    {
        resizeBuffer();
    }
}

void LightVolumeSystem::setMultiSamples(std::uint32_t samples)
{
    if (samples != m_multiSamples)
    {
        m_multiSamples = samples;
        resizeBuffer();
    }
}

//private
void LightVolumeSystem::resizeBuffer()
{
    CRO_ASSERT(m_bufferSize.x > 0 && m_bufferSize.y > 0 && m_bufferScale > 0, "");
    auto size = m_bufferSize / m_bufferScale;

    if (m_shader.getGLHandle())
    {
        const auto usize = glm::vec2(size);
        glUseProgram(m_shader.getGLHandle());
        /*glCheck*/(glUniform2f(m_uniformIDs[UniformID::TargetSize], usize.x, usize.y));
    }

    m_renderTexture.create(size.x, size.y, true, false, m_multiSamples);
    m_renderTexture.setSmooth(true);
}