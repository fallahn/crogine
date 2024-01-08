/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include "../../detail/GLCheck.hpp"

#include <crogine/ecs/systems/DeferredRenderSystem.hpp>

#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/GBuffer.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/util/Matrix.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

#include "../../graphics/shaders/Deferred.hpp"

using namespace cro;

namespace
{
    //TODO Albedo needs to be moved to channel 0, as it is only
    //an 8 bit buffer by default.
    /*
    GBuffer layout:
    Channel 0 - View space normal, RGB
    Channel 1 - View space position, RGB (A) potentially reflectivity? (SSR)
    Channel 2 - Albedo, RGBA
    Channel 3 - PBR mask, RGB(A) potentially emission
    Channel 4 - Accumulated colour for transparency, RGBA
    Channel 5 - R revealage for OIT (G) potentially SSAO blurred buffer
    */

    struct TextureIndex final
    {
        enum
        {
            Normal,
            Position,
            Diffuse,
            Mask,

            Accum,
            Reveal
        };
    };

    std::vector<Colour> ClearColours; /*=
    {
        cro::Colour::White,
        cro::Colour::Black,
        cro::Colour::Transparent,
        cro::Colour::Black,
        cro::Colour::Transparent
    };*/
}

DeferredRenderSystem::DeferredRenderSystem(MessageBus& mb)
    : System        (mb, typeid(DeferredRenderSystem)),
    m_cameraCount   (0),
    m_listIndices   (1),
    m_deferredVao   (0),
    m_forwardVao    (0),
    m_vbo           (0),
    m_envMap        (nullptr)
{
    requireComponent<Model>();
    requireComponent<Transform>();

    //for some reason we can't const initialise this
    //the colours all end up zeroed..?
    ClearColours =
    {
        cro::Colour::Transparent,
        cro::Colour::Black,
        cro::Colour::Transparent,
        cro::Colour::Black,
        cro::Colour::Transparent,
        cro::Colour::White
    };

    setupRenderQuad();
}

DeferredRenderSystem::~DeferredRenderSystem()
{
#ifdef PLATFORM_DESKTOP
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }

    if (m_deferredVao)
    {
        glCheck(glDeleteVertexArrays(1, &m_deferredVao));
    }

    if (m_forwardVao)
    {
        glCheck(glDeleteVertexArrays(1, &m_forwardVao));
    }
#endif
}

//public
void DeferredRenderSystem::updateDrawList(Entity camera)
{
    auto& cam = camera.getComponent<Camera>();
    if (!cam.active)
    {
        return;
    }
    CRO_ASSERT(camera.hasComponent<GBuffer>(), "Needs a GBuffer component to render");

    if (m_listIndices.size() <= cam.getDrawListIndex())
    {
        m_listIndices.resize(cam.getDrawListIndex() + 1);
    }


    auto cameraPos = camera.getComponent<Transform>().getWorldPosition();
    auto& entities = getEntities();

    if (m_visibleLists.size() == m_cameraCount)
    {
        m_visibleLists.emplace_back();
    }
    auto& [deferred, forward] = m_visibleLists[m_cameraCount];
    deferred.clear(); //TODO if we had a dirty flag on the cam transform we could only update this if the camera moved...
    forward.clear();

    for (auto& entity : entities)
    {
        auto& model = entity.getComponent<Model>();
        if (model.isHidden())
        {
            continue;
        }

        //cull entities from frustum - TODO using a dynamic tree could improve this
        //on bigger scenes
        auto sphere = model.m_meshData.boundingSphere;
        const auto& tx = entity.getComponent<Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getScale();
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

        //TODO we *could* do every camera pass - but for now we're
        //expecting screen space reflections rather than using camera reflection mapping

        //for(pass in cam.passes)
        //{
            const auto& frustum = cam.getPass(Camera::Pass::Final).getFrustum();
            auto forwardVector = cro::Util::Matrix::getForwardVector(cam.getPass(Camera::Pass::Final).viewMatrix);

            bool visible = true;
            std::size_t i = 0;
            while (visible && i < frustum.size())
            {
                visible = (Spatial::intersects(frustum[i++], sphere) != Planar::Back);
            }

            if (visible)
            {
                auto direction = (sphere.centre - cameraPos);
                float distance = glm::dot(forwardVector, direction);

                SortData d;
                d.entity = entity;
                d.distanceFromCamera = distance;

                SortData f;
                f.entity = entity;
                d.distanceFromCamera = distance;

                //TODO a large model with a centre behind the camera
                //might still intersect the view but register as being
                //further away than smaller objects in front

                //foreach material
                //add ent/index pair to deferred or forward list
                for (i = 0u; i < model.m_meshData.submeshCount; ++i)
                {
                    if (!model.m_materials[Mesh::IndexData::Final][i].deferred)
                    {
                        f.materialIDs.push_back(static_cast<std::int32_t>(i));
                    }
                    else
                    {
                        d.materialIDs.push_back(static_cast<std::int32_t>(i));
                    }
                }

                //store the entity in the appropriate list
                //if it has any materials associated with it
                if (!d.materialIDs.empty())
                {
                    deferred.push_back(d);
                }

                if (!f.materialIDs.empty())
                {
                    forward.push_back(f);
                }
            }
        //}
    }
    
    //sort deferred list by distance to Camera.
    std::sort(deferred.begin(), deferred.end(),
        [](const SortData& a, const SortData& b)
        {
            return a.distanceFromCamera < b.distanceFromCamera;
        });

    DPRINT("Deferred ents: ", std::to_string(deferred.size()));
    DPRINT("Forward ents: ", std::to_string(forward.size()));

    m_listIndices[cam.getDrawListIndex()] = m_cameraCount;
    m_cameraCount++;
}

void DeferredRenderSystem::render(Entity camera, const RenderTarget& rt)
{
#ifdef PLATFORM_DESKTOP
    const auto& cam = camera.getComponent<Camera>();
    const auto& pass = cam.getPass(Camera::Pass::Final);

    auto listID = m_listIndices[cam.getDrawListIndex()];
    const auto& [deferred, forward] = m_visibleLists[listID];

    glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.08f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

    const auto& camTx = camera.getComponent<Transform>();
    auto cameraPosition = camTx.getWorldPosition();
    auto screenSize = glm::vec2(rt.getSize());

    glCheck(glCullFace(pass.getCullFace()));
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glEnable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_BLEND));

    //render deferred to GBuffer
    auto& buffer = camera.getComponent<GBuffer>().buffer;
    buffer.clear(ClearColours);

    for (auto [entity, matIDs, depth] : deferred)
    {
        //foreach submesh / material:
        const auto& model = entity.getComponent<Model>();

        if ((model.m_renderFlags & pass.renderFlags) == 0)
        {
            continue;
        }

        //calc entity transform
        const auto& tx = entity.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = pass.viewMatrix * worldMat;

        for (auto i : matIDs)
        {
            //bind shader
            glCheck(glUseProgram(model.m_materials[Mesh::IndexData::Final][i].shader));

            //apply shader uniforms from material
            //TODO this does a lot of unnecessary things we need to implement a lighter weight version.
            ModelRenderer::applyProperties(model.m_materials[Mesh::IndexData::Final][i], model, *getScene(), cam);

            //apply standard uniforms - TODO check all these are necessary
            //glCheck(glUniform3f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
            //glCheck(glUniform2f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ScreenSize], screenSize.x, screenSize.y));
            //glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::View], 1, GL_FALSE, glm::value_ptr(pass.viewMatrix)));
            //glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ViewProjection], 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
            glCheck(glUniform4f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ClipPlane], clipPlane[0], clipPlane[1], clipPlane[2], clipPlane[3]));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(cam.getProjectionMatrix())));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::World], 1, GL_FALSE, glm::value_ptr(worldMat)));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
            glCheck(glUniformMatrix3fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Normal], 1, GL_FALSE, glm::value_ptr(glm::inverseTranspose(glm::mat3(worldView)))));

            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindVertexArray(model.m_vaos[i][Mesh::IndexData::Final]));
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));
        }
    }


    //render forward/transparent items to GBuffer 
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDepthMask(GL_FALSE));
    glCheck(glEnable(GL_BLEND)); 

    //set correct blend mode for individual buffers
    glCheck(glBlendFunci(TextureIndex::Accum, GL_ONE, GL_ONE)); //accum
    glCheck(glBlendFunci(TextureIndex::Reveal, GL_ZERO, GL_ONE_MINUS_SRC_COLOR)); //revealage
    glCheck(glBlendEquationi(TextureIndex::Accum, GL_FUNC_ADD));
    glCheck(glBlendEquationi(TextureIndex::Reveal, GL_FUNC_ADD));

    for (auto [entity, matIDs, depth] : forward)
    {
        //foreach submesh / material:
        const auto& model = entity.getComponent<Model>();

        if ((model.m_renderFlags & pass.renderFlags) == 0)
        {
            continue;
        }

        //calc entity transform
        const auto& tx = entity.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = pass.viewMatrix * worldMat;

        for (auto i : matIDs)
        {
            //bind shader
            glCheck(glUseProgram(model.m_materials[Mesh::IndexData::Final][i].shader));

            //apply shader uniforms from material
            ModelRenderer::applyProperties(model.m_materials[Mesh::IndexData::Final][i], model, *getScene(), cam);

            //apply standard uniforms
            glCheck(glUniform3f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
            glCheck(glUniform2f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ScreenSize], screenSize.x, screenSize.y));
            glCheck(glUniform4f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ClipPlane], clipPlane[0], clipPlane[1], clipPlane[2], clipPlane[3]));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::View], 1, GL_FALSE, glm::value_ptr(pass.viewMatrix)));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ViewProjection], 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(cam.getProjectionMatrix())));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::World], 1, GL_FALSE, glm::value_ptr(worldMat)));
            glCheck(glUniformMatrix3fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Normal], 1, GL_FALSE, glm::value_ptr(glm::inverseTranspose(glm::mat3(worldMat)))));

            //and... draw.
            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindVertexArray(model.m_vaos[i][Mesh::IndexData::Final]));
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));
        }
    }

    buffer.display();


    //TODO optionally calc screen space effects (ao, reflections etc)



    //PBR lighting pass of deferred items to render target
    //blend alpha so background shows through
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    CRO_ASSERT(m_envMap, "Env map not set!");

    //TODO can we cache these?
    glm::vec2 size(rt.getSize());
    size.x *= cam.viewport.width;
    size.y *= cam.viewport.height;
    glm::mat4 transform = glm::scale(glm::mat4(1.f), { size.x, size.y, 0.f });
    //transform offset should be correct as Scene::Render has set the viewport already
    glm::mat4 projection = glm::ortho(0.f, size.x, 0.f, size.y, -0.1f, 1.f);

    glCheck(glUseProgram(m_pbrShader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_pbrUniforms[PBRUniformIDs::WorldMat], 1, GL_FALSE, glm::value_ptr(transform)));
    glCheck(glUniformMatrix4fv(m_pbrUniforms[PBRUniformIDs::ProjMat], 1, GL_FALSE, glm::value_ptr(projection)));

    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Diffuse).textureID));

    glCheck(glActiveTexture(GL_TEXTURE1));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Mask).textureID));

    glCheck(glActiveTexture(GL_TEXTURE2));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Normal).textureID));

    glCheck(glActiveTexture(GL_TEXTURE3));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Position).textureID));

    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Diffuse], 0));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Mask], 1));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Normal], 2));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Position], 3));


    const auto& sun = getScene()->getSunlight().getComponent<Sunlight>();
    auto lightDir = sun.getDirection();//PBR compositing is actually done in world space.
    glCheck(glUniform3f(m_pbrUniforms[PBRUniformIDs::LightDirection], lightDir.x, lightDir.y, lightDir.z));
    auto lightCol = sun.getColour().getVec4();
    glCheck(glUniform4f(m_pbrUniforms[PBRUniformIDs::LightColour], lightCol.r, lightCol.g, lightCol.b, lightCol.a));
    //glCheck(glUniform3f(m_pbrUniforms[PBRUniformIDs::CameraWorldPosition], cameraPosition.x, cameraPosition.y, cameraPosition.z));

    glCheck(glActiveTexture(GL_TEXTURE4));
    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_envMap->getIrradianceMap().textureID));

    glCheck(glActiveTexture(GL_TEXTURE5));
    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_envMap->getPrefilterMap().textureID));

    glCheck(glActiveTexture(GL_TEXTURE6));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_envMap->getBRDFMap().textureID));

    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::IrradianceMap], 4));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::PrefilterMap], 5));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::BRDFMap], 6));


    glActiveTexture(GL_TEXTURE7);
    glCheck(glBindTexture(GL_TEXTURE_2D, cam.shadowMapBuffer.getTexture().textureID)); //TODO use shadow map sampler directly?
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::ShadowMap], 7));

    auto invViewMatrix =  glm::inverse(pass.viewMatrix);
    auto lightProjMatrix = cam.getShadowViewProjectionMatrix() * invViewMatrix;
    glCheck(glUniformMatrix4fv(m_pbrUniforms[PBRUniformIDs::InverseViewMat], 1, GL_FALSE, &invViewMatrix[0][0]));
    glCheck(glUniformMatrix4fv(m_pbrUniforms[PBRUniformIDs::LightProjMat], 1, GL_FALSE, &lightProjMatrix[0][0]));

    glCheck(glBindVertexArray(m_deferredVao));
    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));


    //TODO we ought to be drawing the skybox here, but this would mean
    //it's renderer specific instead of generally available in the Scene
    //for other systems.

    //Transparent pass to render target
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Accum).textureID));
    glCheck(glActiveTexture(GL_TEXTURE1));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Reveal).textureID));

    glCheck(glUseProgram(m_oitShader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_oitUniforms[OITUniformIDs::WorldMat], 1, GL_FALSE, glm::value_ptr(transform)));
    glCheck(glUniformMatrix4fv(m_oitUniforms[OITUniformIDs::ProjMat], 1, GL_FALSE, glm::value_ptr(projection)));
    glCheck(glUniform1i(m_oitUniforms[OITUniformIDs::Accum], 0));
    glCheck(glUniform1i(m_oitUniforms[OITUniformIDs::Reveal], 1));

    glCheck(glBindVertexArray(m_forwardVao));
    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    //just reset the camera count - it's only
    //used to track how many visible lists we have
    m_cameraCount = 0;

    glCheck(glBindVertexArray(0));
    glCheck(glUseProgram(0));

    glCheck(glDepthMask(GL_TRUE));
#endif
}

void DeferredRenderSystem::setEnvironmentMap(const EnvironmentMap& map)
{
    m_envMap = &map;
}

//private
bool DeferredRenderSystem::loadPBRShader()
{
    std::fill(m_pbrUniforms.begin(), m_pbrUniforms.end(), -1);

    auto result = m_pbrShader.loadFromString(Shaders::Deferred::LightingVertex, Shaders::Deferred::LightingFragment);
    if (result)
    {
        //load uniform mappings
        const auto& uniforms = m_pbrShader.getUniformMap();
        m_pbrUniforms[PBRUniformIDs::WorldMat] = uniforms.at("u_worldMatrix");
        m_pbrUniforms[PBRUniformIDs::ProjMat] = uniforms.at("u_projectionMatrix");

        if (uniforms.count("u_diffuseMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Diffuse] = uniforms.at("u_diffuseMap");
        }
        if (uniforms.count("u_maskMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Mask] = uniforms.at("u_maskMap");
        }
        if (uniforms.count("u_normalMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Normal] = uniforms.at("u_normalMap");
        }
        if (uniforms.count("u_positionMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Position] = uniforms.at("u_positionMap");
        }



        if (uniforms.count("u_lightDirection"))
        {
            m_pbrUniforms[PBRUniformIDs::LightDirection] = uniforms.at("u_lightDirection");
        }
        if (uniforms.count("u_lightColour"))
        {
            m_pbrUniforms[PBRUniformIDs::LightColour] = uniforms.at("u_lightColour");
        }
        if (uniforms.count("u_cameraWorldPosition"))
        {
            m_pbrUniforms[PBRUniformIDs::CameraWorldPosition] = uniforms.at("u_cameraWorldPosition");
        }


        if (uniforms.count("u_irradianceMap"))
        {
            m_pbrUniforms[PBRUniformIDs::IrradianceMap] = uniforms.at("u_irradianceMap");
        }
        if (uniforms.count("u_prefilterMap"))
        {
            m_pbrUniforms[PBRUniformIDs::PrefilterMap] = uniforms.at("u_prefilterMap");
        }
        if (uniforms.count("u_brdfMap"))
        {
            m_pbrUniforms[PBRUniformIDs::BRDFMap] = uniforms.at("u_brdfMap");
        }

        if (uniforms.count("u_inverseViewMatrix"))
        {
            m_pbrUniforms[PBRUniformIDs::InverseViewMat] = uniforms.at("u_inverseViewMatrix");
        }
        if (uniforms.count("u_lightProjectionMatrix"))
        {
            m_pbrUniforms[PBRUniformIDs::LightProjMat] = uniforms.at("u_lightProjectionMatrix");
        }
        if (uniforms.count("u_shadowMap"))
        {
            m_pbrUniforms[PBRUniformIDs::ShadowMap] = uniforms.at("u_shadowMap");
        }
    }

    return result;
}

bool DeferredRenderSystem::loadOITShader()
{
    std::fill(m_oitUniforms.begin(), m_oitUniforms.end(), -1);

    auto result = m_oitShader.loadFromString(Shaders::Deferred::LightingVertex, Shaders::Deferred::OITOutputFragment);
    if (result)
    {
        const auto& uniforms = m_oitShader.getUniformMap();
        m_oitUniforms[OITUniformIDs::WorldMat] = uniforms.at("u_worldMatrix");
        m_oitUniforms[OITUniformIDs::ProjMat] = uniforms.at("u_projectionMatrix");
        m_oitUniforms[OITUniformIDs::Accum] = uniforms.at("u_accumMap");
        m_oitUniforms[OITUniformIDs::Reveal] = uniforms.at("u_revealMap");
    }

    return result;
}

void DeferredRenderSystem::setupRenderQuad()
{
#ifdef PLATFORM_DESKTOP
    if (loadPBRShader() && loadOITShader())
    {
        glCheck(glGenBuffers(1, &m_vbo));
        glCheck(glGenVertexArrays(1, &m_deferredVao));
        glCheck(glGenVertexArrays(1, &m_forwardVao));

        //0------2
        //|      |
        //|      |
        //1------3

        std::vector<float> verts =
        {
            0.f, 1.f, //0.f, 1.f,
            0.f, 0.f,// 0.f, 0.f,
            1.f, 1.f, //1.f, 1.f,
            1.f, 0.f, //1.f, 0.f
        };
        static constexpr std::uint32_t VertexSize = 2 * sizeof(float);

        //deferred lighting pass
        glCheck(glBindVertexArray(m_deferredVao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, VertexSize * verts.size(), verts.data(), GL_STATIC_DRAW));

        auto attribs = m_pbrShader.getAttribMap();
        glCheck(glEnableVertexAttribArray(attribs[Mesh::Attribute::Position]));
        glCheck(glVertexAttribPointer(attribs[Mesh::Attribute::Position], 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(VertexSize), reinterpret_cast<void*>(static_cast<intptr_t>(0))));
        glCheck(glDisableVertexAttribArray(attribs[Mesh::Attribute::Position]));
        glCheck(glEnableVertexAttribArray(0));

        //forward pass
        glCheck(glBindVertexArray(m_forwardVao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
        
        attribs = m_oitShader.getAttribMap();
        glCheck(glEnableVertexAttribArray(attribs[Mesh::Attribute::Position]));
        glCheck(glVertexAttribPointer(attribs[Mesh::Attribute::Position], 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(VertexSize), reinterpret_cast<void*>(static_cast<intptr_t>(0))));
        glCheck(glDisableVertexAttribArray(attribs[Mesh::Attribute::Position]));
        glCheck(glEnableVertexAttribArray(0));

        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
#endif
}