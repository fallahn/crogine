/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/RenderTarget.hpp>
#include <crogine/util/Rectangle.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/Console.hpp>

#include "../../detail/GLCheck.hpp"
#include "../../graphics/shaders/Sprite.hpp"

#include <string>

namespace
{
    //std::vector<float> buns =
    //{
    //    0.f,0.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
    //    0.f,100.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
    //    100.f,0.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
    //    100.f,100.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
    //};
}

using namespace cro;

RenderSystem2D::RenderSystem2D(MessageBus& mb)
    : System        (mb, typeid(RenderSystem2D)),
    m_sortOrder     (DepthAxis::Z),
    m_needsSort     (true),
    m_drawLists     (1)
{
    requireComponent<Drawable2D>();
    requireComponent<Transform>();

    //load default shaders
    m_colouredShader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Coloured);
    m_texturedShader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Textured, "#define TEXTURED\n");
}

RenderSystem2D::~RenderSystem2D()
{
    //tidy up any remaining drawables
    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        resetDrawable(entity);
    }
}

//public
void RenderSystem2D::updateDrawList(Entity camEnt)
{
    auto& camera = camEnt.getComponent<Camera>();
    CRO_ASSERT(camera.isOrthographic(), "Camera is not orthographic");

    if (m_drawLists.size() <= camera.getDrawListIndex())
    {
        m_drawLists.resize(camera.getDrawListIndex() + 1);
    }
    auto& drawlist = m_drawLists[camera.getDrawListIndex()];
    drawlist.clear();

    auto viewRect = camEnt.getComponent<cro::Transform>().getWorldTransform() * camera.getViewSize();


    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& drawable = entity.getComponent<Drawable2D>();
        drawable.m_wasCulledLastFrame = true;

        if ((camera.getPass(Camera::Pass::Final).renderFlags & drawable.m_renderFlags) == 0)
        {
            continue;
        }

        if (drawable.m_autoCrop)
        {
            auto scale = entity.getComponent<cro::Transform>().getWorldScale();
            if (scale.x * scale.y != 0)
            {
                const auto worldMat = entity.getComponent<cro::Transform>().getWorldTransform();

                //check local bounds for visibility and draw if visible
                auto bounds = drawable.m_localBounds.transform(worldMat);
                if (bounds.intersects(viewRect))
                {
                    drawlist.push_back(entity);
                    drawable.m_wasCulledLastFrame = false;
                }
            }
        }
        else
        {
            drawlist.push_back(entity);
            drawable.m_wasCulledLastFrame = false;
        }
    }

    DPRINT("Visible 2D ents", std::to_string(drawlist.size()));

    if (m_needsSort)
    {
        std::sort(drawlist.begin(), drawlist.end(),
            [](Entity a, Entity b)
            {
                return a.getComponent<Drawable2D>().m_sortCriteria < b.getComponent<Drawable2D>().m_sortCriteria;
            });
    }
}

void RenderSystem2D::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& drawable = entity.getComponent<Drawable2D>();
        //check shader flag and set correct shader if needed
        if (drawable.m_applyDefaultShader)
        {
            if (drawable.m_textureInfo.textureID.textureID)
            {
                //use textured shader
                drawable.m_shader = &m_texturedShader;
            }
            else
            {
                //vertex colour
                drawable.m_shader = &m_colouredShader;
            }
            drawable.m_applyDefaultShader = false;
            drawable.applyShader();
        }

        //check data flag and update buffer if needed
        if (drawable.m_updateBufferData)
        {
            //bind VBO and upload data
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, drawable.m_vbo));
            glCheck(glBufferData(GL_ARRAY_BUFFER, drawable.m_vertices.size() * Vertex2D::Size, drawable.m_vertices.data(), GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            drawable.m_updateBufferData = false;
        }

        const auto& tx = entity.getComponent<Transform>();
        auto pos = tx.getWorldPosition();

        //set sort criteria based on position
        //faster to do a sort on int than float
        if (m_sortOrder == DepthAxis::Y)
        {
            //multiplying by 100 preserves two places of precision
            //which is enough to sort on
            drawable.m_sortCriteria = static_cast<std::int32_t>(-pos.y * 100.f);
        }
        else
        {
            drawable.m_sortCriteria = static_cast<std::int32_t>(pos.z * 100.f);
        }

        //check if the cropping area is smaller than
        //the local bounds and update cropping properties
        drawable.m_cropped = !Util::Rectangle::contains(drawable.m_croppingArea, drawable.m_localBounds);

        if (drawable.m_cropped)
        {
            drawable.m_croppingWorldArea = drawable.m_croppingArea.transform(tx.getWorldTransform());
        }
    }

    //for (auto& list : m_drawLists)
    //{
    //    list.erase(std::remove_if(list.begin(), list.end(), [](cro::Entity e) {return !e.isValid(); }), list.end());
    //}
}

void RenderSystem2D::render(Entity cameraEntity, const RenderTarget& rt)
{
    const auto& camComponent = cameraEntity.getComponent<Camera>();
    if (camComponent.getDrawListIndex() < m_drawLists.size())
    {
        const auto& pass = camComponent.getActivePass();
        auto viewport = rt.getViewport(camComponent.viewport);

        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glEnable(GL_SCISSOR_TEST));

        std::uint32_t lastProgram = 0;

        const auto& entities = m_drawLists[camComponent.getDrawListIndex()];
        for (auto entity : entities)
        {
#ifdef CRO_DEBUG_
            //these are probably OK to draw as they aren't yet cleared up
            //(just marked for removal) but it will ASSERT on debug builds
            if (!entity.isValid()) continue;
#endif

            const auto& drawable = entity.getComponent<Drawable2D>();
            const auto& tx = entity.getComponent<cro::Transform>();
            glm::mat4 worldMat = tx.getWorldTransform();

            if (//TODO surely these ought to be culling criteria?
                drawable.m_shader && !drawable.m_updateBufferData)
            {
                //apply shader
                auto program = drawable.m_shader->getGLHandle();
                if (program != lastProgram)
                {
                    glCheck(glUseProgram(program));
                    lastProgram = program;
                }
                //glCheck(glUniformMatrix4fv(drawable.m_worldUniform, 1, GL_FALSE, &(worldMat[0].x)));
                glCheck(glUniformMatrix4fv(drawable.m_viewProjectionUniform, 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
                glCheck(glUniformMatrix4fv(drawable.m_worldUniform, 1, GL_FALSE, glm::value_ptr(worldMat)));

                //apply texture if active
                if (drawable.m_textureInfo.textureID.textureID)
                {
                    glCheck(glActiveTexture(GL_TEXTURE0));
                    glCheck(glBindTexture(GL_TEXTURE_2D, drawable.m_textureInfo.textureID.textureID));
                    glCheck(glUniform1i(drawable.m_textureUniform, 0));
                }

                //apply any custom uniforms
                std::int32_t j = 1;
                for (const auto& [uniform, value] : drawable.m_textureIDBindings)
                {
                    glCheck(glActiveTexture(GL_TEXTURE0 + j));
                    glCheck(glBindTexture(GL_TEXTURE_2D, value));
                    glCheck(glUniform1i(uniform, j));
                    j++;
                }
                for (auto [uniform, value] : drawable.m_floatBindings)
                {
                    glCheck(glUniform1f(uniform, value));
                }
                for (auto [uniform, value] : drawable.m_vec2Bindings)
                {
                    glCheck(glUniform2f(uniform, value.x, value.y));
                }
                for (auto [uniform, value] : drawable.m_vec3Bindings)
                {
                    glCheck(glUniform3f(uniform, value.x, value.y, value.z));
                }
                for (auto [uniform, value] : drawable.m_vec4Bindings)
                {
                    glCheck(glUniform4f(uniform, value.r, value.g, value.b, value.a));
                }
                for (auto [uniform, value] : drawable.m_boolBindings)
                {
                    glCheck(glUniform1i(uniform, value));
                }
                for (const auto& [uniform, value] : drawable.m_matBindings)
                {
                    glCheck(glUniformMatrix4fv(uniform, 1, GL_FALSE, value));
                }

                applyBlendMode(drawable.m_blendMode);

                if (drawable.m_cropped)
                {
                    //convert cropping area to target coords (remember this might not be a window!)
                    glm::vec2 start(drawable.m_croppingWorldArea.left, drawable.m_croppingWorldArea.bottom);
                    glm::vec2 end(start.x + drawable.m_croppingWorldArea.width, start.y + drawable.m_croppingWorldArea.height);

                    auto scissorStart = mapCoordsToPixel(start, pass.viewProjectionMatrix, viewport);
                    auto scissorEnd = mapCoordsToPixel(end, pass.viewProjectionMatrix, viewport);

                    auto w = scissorEnd.x - scissorStart.x;
                    auto h = scissorEnd.y - scissorStart.y;
                    
                    glCheck(glScissor(scissorStart.x, scissorStart.y, w, h));
                }
                else
                {
                    auto rtSize = rt.getSize();
                    glCheck(glScissor(0, 0, rtSize.x, rtSize.y));
                }

                glCheck(glFrontFace(drawable.m_facing));

#ifdef PLATFORM_DESKTOP
                glCheck(glBindVertexArray(drawable.m_vao));
                glCheck(glDrawArrays(static_cast<GLenum>(drawable.m_primitiveType), 0, static_cast<GLsizei>(drawable.m_vertices.size())));

#else //GLES 2 doesn't have VAO support without extensions
                glCheck(glBindBuffer(GL_ARRAY_BUFFER, drawable.m_vbo));

                //bind attribs
                //const auto& attribs = drawable.m_vertexAttribs;
                for (const auto& [id, size, offset] : drawable.m_vertexAttributes)
                {
                    glCheck(glEnableVertexAttribArray(id));
                    glCheck(glVertexAttribPointer(id, size,
                        GL_FLOAT, GL_FALSE, static_cast<GLsizei>(Vertex2D::Size),
                        reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
                }

                //draw array
                glCheck(glDrawArrays(static_cast<GLenum>(drawable.m_primitiveType), 0, drawable.m_vertices.size()));

                //and unbind... this could be saved by only changing when switching shader
                for (const auto& attrib : drawable.m_vertexAttributes)
                {
                    glCheck(glDisableVertexAttribArray(attrib.id));
                }

#endif //PLATFORM 
            }
        }

#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif
        glCheck(glUseProgram(0));

        glCheck(glDisable(GL_SCISSOR_TEST));
        applyBlendMode(Material::BlendMode::None);
        glCheck(glDisable(GL_CULL_FACE));
        glCheck(glFrontFace(GL_CCW));
        glCheck(glDepthMask(GL_TRUE));
        //glCheck(glCullFace(GL_BACK));
    }
}

void RenderSystem2D::setSortOrder(DepthAxis sortOrder)
{
    m_sortOrder = sortOrder;
}

const std::string& RenderSystem2D::getDefaultVertexShader()
{
    return Shaders::Sprite::Vertex;
}

//private
void RenderSystem2D::applyBlendMode(Material::BlendMode blendMode)
{
    switch (blendMode)
    {
    default: break;
    case Material::BlendMode::Additive:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Alpha:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::None:
        glCheck(glDisable(GL_BLEND));
        break;
    }
}

glm::ivec2 RenderSystem2D::mapCoordsToPixel(glm::vec2 coord, const glm::mat4& viewProjectionMatrix, IntRect viewport) const
{
    auto worldPoint = viewProjectionMatrix * glm::vec4(coord, 0.f, 1.f);

    glm::ivec2 retVal(static_cast<int>((worldPoint.x + 1.f) / 2.f * viewport.width + viewport.left), 
                      static_cast<int>((worldPoint.y + 1.f) / 2.f * viewport.height + viewport.bottom));

    return retVal;
}

void RenderSystem2D::onEntityAdded(Entity entity)
{
    //create the VBO (VAO is applied when shader is set)
    auto& drawable = entity.getComponent<Drawable2D>();
    if (drawable.m_vbo == 0) //setting a custom shader may have already created this
    {
        glCheck(glGenBuffers(1, &drawable.m_vbo));
    }

    entity.getComponent<cro::Transform>().addCallback(
        [&]()
        {
            m_needsSort = true;
        });
    m_needsSort = true;
}

void RenderSystem2D::onEntityRemoved(Entity entity)
{
    //remove any OpenGL buffers
    resetDrawable(entity);
}

void RenderSystem2D::resetDrawable(Entity entity)
{
    auto& drawable = entity.getComponent<Drawable2D>();
    if (drawable.m_vbo != 0)
    {
        glCheck(glDeleteBuffers(1, &drawable.m_vbo));
    }

#ifdef PLATFORM_DESKTOP

    if (drawable.m_vao != 0)
    {
        glCheck(glDeleteVertexArrays(1, &drawable.m_vao));
    }

#endif //PLATFORM
}