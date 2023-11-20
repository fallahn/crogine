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

#include <crogine/core/App.hpp>
#include <crogine/core/Message.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/LightVolume.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/systems/LightVolumeSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/Spatial.hpp>
#include "../../detail/GLCheck.hpp"

#ifdef CRO_DEBUG_
#include <crogine/gui/Gui.hpp>
#endif

using namespace cro;

namespace
{
    //TODO worth adding to Util?
    static inline constexpr float smoothstep(float edge0, float edge1, float x)
    {
        float t = std::min(1.f, std::max(0.f, (x - edge0) / (edge1 - edge0)));
        return t * t * (3.f - 2.f * t);
    }

    const std::string VertexShader = 
        R"(
        ATTRIBUTE vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;
#if defined (WORLD_SPACE)
        VARYING_OUT vec3 v_lightPosition;
#endif
        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;
#if defined (WORLD_SPACE)
            v_lightPosition = vec3(u_worldMatrix[3]);
#endif
        })";

    const std::string FragmentShader = 
        R"(
        OUTPUT

        uniform sampler2D u_normalMap;
        uniform sampler2D u_positionMap;

        uniform float u_lightRadiusSqr;
        uniform vec3 u_lightColour = vec3(1.0, 1.0, 0.0);

        uniform vec3 u_lightPos;
        uniform vec2 u_targetSize = vec2(640.0, 480.0);

#if defined (WORLD_SPACE)
        VARYING_IN vec3 v_lightPosition;
#endif

        const float ColourSteps = 20.0;

        void main()
        {
            vec2 texCoord = gl_FragCoord.xy / u_targetSize;

            vec4 normalSample = TEXTURE(u_normalMap, texCoord);
            vec3 normal = normalize(normalSample.rgb); //normalSample.a is self-illum mask
            vec3 position = TEXTURE(u_positionMap, texCoord).rgb;

#if defined(WORLD_SPACE)
            vec3 lightDir = v_lightPosition - position;
#else
            vec3 lightDir = u_lightPos - position;
#endif
            
            float amount = dot(normal, normalize(lightDir));

            /*amount *= ColourSteps;
            amount = round(amount);
            amount /= ColourSteps;*/

            vec3 lightColour = u_lightColour * max(amount, 0.0) * normalSample.a;

            //not perfectly accurate compared to the linear/quadratic equation but easier to involve the radius 
            float attenuation = 1.0 - min(dot(lightDir, lightDir) / u_lightRadiusSqr, 1.0);

            lightColour *= attenuation;
            FRAG_OUT = vec4(lightColour, 1.0);
        })";
}

LightVolumeSystem::LightVolumeSystem(MessageBus& mb, std::int32_t spaceIndex)
    : System        (mb, typeid(LightVolumeSystem)),
    m_spaceIndex    (spaceIndex),
    m_drawLists     (20)
{
    requireComponent<LightVolume>();
    requireComponent<Model>();
    requireComponent<Transform>();

    std::fill(m_uniformIDs.begin(), m_uniformIDs.end(), -1);

    bool loaded = false;

    switch (spaceIndex)
    {
    default: assert(false); break;
    case LightVolume::ViewSpace:
        loaded = m_shader.loadFromString(VertexShader, FragmentShader);
        break;
    case LightVolume::WorldSpace:
        loaded = m_shader.loadFromString(VertexShader, FragmentShader, "#define WORLD_SPACE\n");
        break;
    }

    if (loaded)
    {
        m_uniformIDs[UniformID::World] = m_shader.getUniformID("u_worldMatrix");
        //m_uniformIDs[UniformID::View] = m_shader.getUniformID("u_viewMatrix");
        m_uniformIDs[UniformID::ViewProjection] = m_shader.getUniformID("u_viewProjectionMatrix");

        m_uniformIDs[UniformID::NormalMap] = m_shader.getUniformID("u_normalMap");
        m_uniformIDs[UniformID::PositionMap] = m_shader.getUniformID("u_positionMap");

        m_uniformIDs[UniformID::TargetSize] = m_shader.getUniformID("u_targetSize");

        m_uniformIDs[UniformID::LightColour] = m_shader.getUniformID("u_lightColour");
        m_uniformIDs[UniformID::LightRadiusSqr] = m_shader.getUniformID("u_lightRadiusSqr");
        m_uniformIDs[UniformID::LightPosition] = m_shader.getUniformID("u_lightPos");
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
}

//public
void LightVolumeSystem::process(float)
{
    for (auto entity : getEntities())
    {
        const auto& model = entity.getComponent<Model>();
        auto sphere = model.getBoundingSphere();
        const auto& tx = entity.getComponent<Transform>();

        auto scale = tx.getWorldScale();
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

        entity.getComponent<LightVolume>().lightScale = sphere.radius / entity.getComponent<LightVolume>().radius;
    }
}

void LightVolumeSystem::updateDrawList(Entity cameraEnt)
{
    const auto& camComponent = cameraEnt.getComponent<Camera>();
    const auto& frustum = camComponent.getPass(Camera::Pass::Final).getFrustum();
    const auto cameraPos = cameraEnt.getComponent<Transform>().getWorldPosition();
    const auto& entities = getEntities();

    if (m_drawLists.size() <= camComponent.getDrawListIndex())
    {
        m_drawLists.resize(camComponent.getDrawListIndex() + 1);
    }

    auto& drawList = m_drawLists[camComponent.getDrawListIndex()];
    drawList.clear();

    //TODO this only does lighting on the output pass, not the reflection
    //though this is probably enough for our case

    //TODO - and this goes for all culling functions
    //the world space transform of the sphere could be cached for a frame
    //instead of being recalculated for every active camera
    for (auto entity : entities)
    {
        const auto& model = entity.getComponent<Model>();
        auto sphere = model.getBoundingSphere();
        const auto& tx = entity.getComponent<Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getWorldScale();

        if (scale.x * scale.y * scale.z == 0)
        {
            continue;
        }

        //average for non-uniform scale
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

        const auto direction = (sphere.centre - cameraPos);
        const float distance = glm::dot(camComponent.getPass(Camera::Pass::Final).forwardVector, direction);

        if (distance < -sphere.radius)
        {
            //model is behind the camera
            continue;
        }

        auto& light = entity.getComponent<LightVolume>();
        const auto l2 = glm::length2(direction);
        if (l2 >light.maxVisibilityDistance)
        {
            //model is out of bounds
            continue;
        }

        bool visible = true;
        std::size_t j = 0;
        while (visible && j < frustum.size())
        {
            visible = (Spatial::intersects(frustum[j++], sphere) != Planar::Back);
        }

        if (visible)
        {
            light.cullAttenuation = 1.f - smoothstep(light.maxVisibilityDistance - (light.maxVisibilityDistance * 0.33f), light.maxVisibilityDistance, l2);
            drawList.push_back(entity);
        }
    }
}

void LightVolumeSystem::updateTarget(Entity camera, RenderTexture& target)
{
    const auto& camComponent = camera.getComponent<Camera>();
    const auto& pass = camComponent.getPass(Camera::Pass::Final);
    const auto& camTx = camera.getComponent<Transform>();
    const auto cameraPosition = camTx.getWorldPosition();

    CRO_ASSERT(camComponent.getDrawListIndex() < m_drawLists.size(), "Can't call this before having updated draw lists");
    const auto& drawList = m_drawLists[camComponent.getDrawListIndex()];

    glCheck(glFrontFace(GL_CCW));
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glCullFace(GL_FRONT));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_bufferIDs[BufferID::Normal].textureID);

    glActiveTexture(GL_TEXTURE0+1);
    glBindTexture(GL_TEXTURE_2D, m_bufferIDs[BufferID::Position].textureID);

    const auto viewProj = camComponent.getProjectionMatrix() * pass.viewMatrix;
    const glm::vec2 size = glm::vec2(target.getSize());

    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniform2f(m_uniformIDs[UniformID::TargetSize], size.x, size.y));
    glCheck(glUniform1i(m_uniformIDs[UniformID::NormalMap], 0));
    glCheck(glUniform1i(m_uniformIDs[UniformID::PositionMap], 1));
    //glCheck(glUniformMatrix4fv(m_uniformIDs[UniformID::View], 1, GL_FALSE, &pass.viewMatrix[0][0]));
    glCheck(glUniformMatrix4fv(m_uniformIDs[UniformID::ViewProjection], 1, GL_FALSE, &viewProj[0][0]));

    //if there are multiple lights blend additively
    //TODO we probably want to sort back to front when culling
    glCheck(glEnable(GL_BLEND));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glBlendFunc(GL_ONE, GL_ONE));
    glCheck(glBlendEquation(GL_FUNC_ADD));

    target.clear();
    for (const auto& entity : drawList)
    {
        const auto& tx = entity.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glCheck(glUniformMatrix4fv(m_uniformIDs[UniformID::World], 1, GL_FALSE, &worldMat[0][0]));

        if (m_spaceIndex == LightVolume::ViewSpace)
        {
            const auto viewPos = glm::vec3(pass.viewMatrix * glm::vec4(tx.getWorldPosition(), 1.f));
            glCheck(glUniform3f(m_uniformIDs[UniformID::LightPosition], viewPos.x, viewPos.y, viewPos.z));
        }

        const auto& light = entity.getComponent<LightVolume>();
        float radius = light.radius * light.lightScale;
        glm::vec4 colour = light.colour.getVec4() * light.cullAttenuation;
        glCheck(glUniform3f(m_uniformIDs[UniformID::LightColour], colour.r, colour.g, colour.b));
        glCheck(glUniform1f(m_uniformIDs[UniformID::LightRadiusSqr], radius * radius));

        entity.getComponent<Model>().draw(0, Mesh::IndexData::Final);
    }
    target.display();

    glCheck(glCullFace(GL_BACK));
    glCheck(glFrontFace(GL_CCW));
    glCheck(glDisable(GL_BLEND));
    glCheck(glDisable(GL_CULL_FACE));
    //glCheck(glEnable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_TRUE));


    //glCheck(glUseProgram(0));
}

void LightVolumeSystem::setSourceBuffer(TextureID id, std::int32_t index)
{
    CRO_ASSERT(index != -1 && index < BufferID::Count, "");
    m_bufferIDs[index] = id;
}