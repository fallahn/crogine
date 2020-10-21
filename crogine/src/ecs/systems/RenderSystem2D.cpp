/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "../../detail/glad.hpp"
#include "../../detail/GLCheck.hpp"

#include <string>

namespace
{
    const std::string VertexShader = 
        R"(
            uniform mat4 u_worldViewMatrix;
            uniform mat4 u_projectionMatrix;

            ATTRIBUTE vec2 a_position;
            ATTRIBUTE MED vec2 a_texCoord0;
            ATTRIBUTE LOW vec4 a_colour;

            VARYING_OUT LOW vec4 v_colour;

            #if defined(TEXTURED)
            VARYING_OUT MED vec2 v_texCoord;
            #endif

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldViewMatrix * vec4(a_position, 0.0, 1.0);
                v_colour = a_colour;
            #if defined(TEXTURED)
                v_texCoord = a_texCoord0;
            #endif
            })";

    const std::string ColouredFragmentShader = 
        R"(
            VARYING_IN LOW vec4 v_colour;
            OUTPUT
            
            void main()
            {
                FRAG_OUT  = v_colour;
            })";

    const std::string TexturedFragmentShader = 
        R"(
            uniform sampler2D u_texture;

            VARYING_IN LOW vec4 v_colour;
            VARYING_IN MED vec2 v_texCoord;
            OUTPUT
            
            void main()
            {
                FRAG_OUT  = TEXTURE(u_texture, v_texCoord) * v_colour;
            })";

    std::vector<float> buns =
    {
        0.f,0.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
        0.f,100.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
        100.f,0.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
        100.f,100.f,  0.f,0.f, 1.f,0.f,0.f,1.f,
    };
}

using namespace cro;

RenderSystem2D::RenderSystem2D(MessageBus& mb)
    : System    (mb, typeid(RenderSystem2D)),
    m_sortOrder (DepthAxis::Z)
{
    requireComponent<Drawable2D>();
    requireComponent<Transform>();

    //load default shaders
    m_colouredShader.loadFromString(VertexShader, ColouredFragmentShader);
    m_texturedShader.loadFromString(VertexShader, TexturedFragmentShader, "#define TEXTURED\n");
}

RenderSystem2D::~RenderSystem2D()
{
    //tidy up any remaining drawables
    for (auto entity : getEntities())
    {
        resetDrawable(entity);
    }
}

//public
void RenderSystem2D::updateDrawList(Entity camEnt)
{
    std::vector<Entity> drawList;
    auto& camera = camEnt.getComponent<Camera>();
    const auto& frustum = camera.getFrustum();

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& drawable = entity.getComponent<Drawable2D>();
        const auto worldMat = entity.getComponent<cro::Transform>().getWorldTransform();

        //check local bounds for visibility and draw if visible
        auto bounds = drawable.m_localBounds.transform(worldMat);
        cro::Box aabb(glm::vec3(bounds.left, bounds.bottom, -0.1f), glm::vec3(bounds.left + bounds.width, bounds.bottom + bounds.height, 0.1f));

        bool visible = true;
        std::size_t i = 0;
        while (visible && i < frustum.size())
        {
            visible = (Spatial::intersects(frustum[i++], aabb) != Planar::Back);
        }

        if (visible)
        {
            drawList.push_back(entity);
        }
    }

    DPRINT("Visible 2D ents", std::to_string(drawList.size()));

    //sort drawlist
    std::sort(drawList.begin(), drawList.end(),
        [](Entity a, Entity b)
        {
            return a.getComponent<Drawable2D>().m_sortCriteria < b.getComponent<Drawable2D>().m_sortCriteria;
        });
 
    camera.drawList[getType()] = std::make_any<std::vector<Entity>>(std::move(drawList));
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
            if (drawable.m_texture)
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
            
            auto scale = tx.getScale();

            //update world positions - using world tx directly doesn't account for orignal offset
            drawable.m_croppingWorldArea = drawable.m_croppingArea;
            drawable.m_croppingWorldArea.left += pos.x;
            drawable.m_croppingWorldArea.bottom += pos.y;
            drawable.m_croppingWorldArea.width *= scale.x;
            drawable.m_croppingWorldArea.height *= scale.y;
        }
    }
}

void RenderSystem2D::render(Entity cameraEntity, const RenderTarget& rt)
{
    const auto& camComponent = cameraEntity.getComponent<Camera>();
    auto viewport = rt.getViewport(camComponent.viewport);

    glCheck(glDepthMask(GL_FALSE));
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glEnable(GL_SCISSOR_TEST));

    const auto& entities = std::any_cast<const std::vector<Entity>&>(camComponent.drawList.at(getType()));
    for (auto entity : entities)
    {
        const auto& drawable = entity.getComponent<Drawable2D>();
        const auto& tx = entity.getComponent<cro::Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();

        if (drawable.m_shader && !drawable.m_updateBufferData)
        {
            //apply shader
            glm::mat4 worldView = camComponent.viewMatrix * worldMat;

            glCheck(glUseProgram(drawable.m_shader->getGLHandle()));
            //glCheck(glUniformMatrix4fv(drawable.m_worldUniform, 1, GL_FALSE, &(worldMat[0].x)));
            glCheck(glUniformMatrix4fv(drawable.m_worldViewUniform, 1, GL_FALSE, glm::value_ptr(worldView)));
            glCheck(glUniformMatrix4fv(drawable.m_projectionUniform, 1, GL_FALSE, glm::value_ptr(camComponent.projectionMatrix)));

            //apply texture if active
            if (drawable.m_texture)
            {
                glCheck(glActiveTexture(GL_TEXTURE0));
                glCheck(glBindTexture(GL_TEXTURE_2D, drawable.m_texture->getGLHandle()));
                glCheck(glUniform1i(drawable.m_textureUniform, 0));
            }
            
            //apply any custom uniforms
            std::int32_t j = 1;
            for (const auto& [uniform, value] : drawable.m_textureBindings)
            {
                glCheck(glActiveTexture(GL_TEXTURE0 + j));
                glCheck(glBindTexture(GL_TEXTURE_2D, value->getGLHandle()));
                glCheck(glUniform1i(uniform, j));
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
            for (const auto [uniform, value] : drawable.m_matBindings)
            {
                glCheck(glUniformMatrix4fv(uniform, 1, GL_FALSE, value));
            }

            applyBlendMode(drawable.m_blendMode);
            
            if (drawable.m_cropped)
            {
                //convert cropping area to target coords (remember this might not be a window!)
                glm::vec2 start(drawable.m_croppingWorldArea.left, drawable.m_croppingWorldArea.bottom);
                glm::vec2 end(start.x + drawable.m_croppingWorldArea.width, start.y + drawable.m_croppingWorldArea.height);

                auto scissorStart = mapCoordsToPixel(start, camComponent.viewProjectionMatrix, viewport);
                auto scissorEnd = mapCoordsToPixel(end, camComponent.viewProjectionMatrix, viewport);

                glCheck(glScissor(scissorStart.x, scissorStart.y, scissorEnd.x - scissorStart.x, scissorEnd.y - scissorStart.y));
            }
            else
            {
                auto rtSize = rt.getSize();
                glCheck(glScissor(0, 0, rtSize.x, rtSize.y));
            }

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
    glCheck(glDepthMask(GL_TRUE));
}

void RenderSystem2D::setSortOrder(DepthAxis sortOrder)
{
    m_sortOrder = sortOrder;
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