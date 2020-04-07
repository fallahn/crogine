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
#include <crogine/detail/glm/gtc/type_ptr.hpp>

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
void RenderSystem2D::process(float)
{
    bool needsSorting = false; 

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& drawable = entity.getComponent<Drawable2D>();
        //check shader flag and set correct shader if needed
        //TODO also flag if custom shader so applyShader() is
        //only ever called from this point
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

        //check the transform to see if it changed and triggered a
        //sort update for the entity list
        const auto& tx = entity.getComponent<Transform>();
        float newSort = 0.f;
        if (m_sortOrder == DepthAxis::Y)
        {
            newSort = tx.getWorldPosition().y;
        }
        else
        {
            newSort = tx.getWorldPosition().z;
        }

        if (drawable.m_lastSortValue != newSort)
        {
            needsSorting = true;
        }
        drawable.m_lastSortValue = newSort;
    }

    //sort drawlist depending on Z or Y sort
    if (needsSorting)
    {
        if (m_sortOrder == DepthAxis::Y)
        {
            std::sort(entities.begin(), entities.end(),
                [](Entity a, Entity b)
                {
                    return a.getComponent<cro::Transform>().getWorldPosition().y > b.getComponent<cro::Transform>().getWorldPosition().y;
                });
        }
        else
        {
            std::sort(entities.begin(), entities.end(),
                [](Entity a, Entity b)
                {
                    return a.getComponent<cro::Transform>().getWorldPosition().z < b.getComponent<cro::Transform>().getWorldPosition().z;
                });
        }
    }
}

void RenderSystem2D::render(Entity cameraEntity)
{
    const auto& camComponent = cameraEntity.getComponent<Camera>();
    auto frustum = camComponent.getFrustum();

    glCheck(glDepthMask(GL_FALSE));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_DEPTH_TEST));

    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        const auto& drawable = entity.getComponent<Drawable2D>();
        const auto& tx = entity.getComponent<cro::Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();

        //check local bounds for visibility and draw if visible
        auto bounds = drawable.m_localBounds.transform(worldMat);
        cro::Box aabb(glm::vec3(bounds.left, bounds.bottom, -0.1f), glm::vec3(bounds.left + bounds.width, bounds.bottom + bounds.height, 0.1f));
        auto pos = tx.getWorldPosition();
        pos.z = 0.f;
        aabb += pos;

        bool visible = true;
        std::size_t i = 0;
        while (visible && i < frustum.size())
        {
            visible = (Spatial::intersects(frustum[i++], aabb) != Planar::Back);
        }

        if (visible && drawable.m_shader)
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
                glCheck(glUniform1i(drawable.m_textureUniform, 0));
                glCheck(glBindTexture(GL_TEXTURE_2D, drawable.m_texture->getGLHandle()));
            }
            
            //TODO if this is a custom shader we want to apply other uniforms here

            applyBlendMode(drawable.m_blendMode);
            
#ifdef PLATFORM_DESKTOP
            glCheck(glBindVertexArray(drawable.m_vao));
            glCheck(glDrawArrays(static_cast<GLenum>(drawable.m_primitiveType), 0, static_cast<GLsizei>(drawable.m_vertices.size())));

#else //GLES 2 doesn't have VAO support without extensions
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, drawable.m_vbo));

            //bind attribs
            const auto& attribs = drawable.m_shader->getAttribMap();

            //position attrib
            glCheck(glEnableVertexAttribArray(attribs[Mesh::Attribute::Position]));
            glCheck(glVertexAttribPointer(attribs[Mesh::Attribute::Position], 2,
                GL_FLOAT, GL_FALSE, static_cast<GLsizei>(Vertex2D::Size),
                reinterpret_cast<void*>(static_cast<intptr_t>(0))));

            //UV attrib - only exists on textured shaders
            if (attribs[Mesh::Attribute::UV0] != -1)
            {
                glCheck(glEnableVertexAttribArray(attribs[Mesh::Attribute::UV0]));
                glCheck(glVertexAttribPointer(attribs[Mesh::Attribute::UV0], 2,
                    GL_FLOAT, GL_FALSE, static_cast<GLsizei>(Vertex2D::Size),
                    reinterpret_cast<void*>(static_cast<intptr_t>(2 * sizeof(float)))));
            }

            //colour attrib
            glCheck(glEnableVertexAttribArray(attribs[Mesh::Attribute::Colour]));
            glCheck(glVertexAttribPointer(attribs[Mesh::Attribute::Colour], 4,
                GL_FLOAT, GL_FALSE, static_cast<GLsizei>(Vertex2D::Size),
                reinterpret_cast<void*>(static_cast<intptr_t>(4 * sizeof(float))))); //offset from beginning of vertex, not size!

            //draw array
            glCheck(glDrawArrays(static_cast<GLenum>(drawable.m_primitiveType), 0, drawable.m_vertices.size()));


            //unbind attribs
            glCheck(glDisableVertexAttribArray(attribs[Mesh::Attribute::Position]));
            if (attribs[Mesh::Attribute::UV0] != -1) glCheck(glDisableVertexAttribArray(attribs[Mesh::Attribute::UV0]));
            glCheck(glDisableVertexAttribArray(attribs[Mesh::Attribute::Colour]));

#endif //PLATFORM 
        }
    }

#ifdef PLATFORM_DESKTOP
    glCheck(glBindVertexArray(0));
#else
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif
    glCheck(glUseProgram(0));

    applyBlendMode(Material::BlendMode::None);
    //glCheck(glDisable(GL_CULL_FACE));
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
        //glCheck(glEnable(GL_DEPTH_TEST));
        //glCheck(glDepthMask(GL_FALSE));
        //glCheck(glEnable(GL_CULL_FACE));
        break;
    case Material::BlendMode::Alpha:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        //glCheck(glDisable(GL_CULL_FACE));
        //glCheck(glDisable(GL_DEPTH_TEST));
        //glCheck(glEnable(GL_DEPTH_TEST));
        //glCheck(glDepthMask(GL_FALSE));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        //glCheck(glEnable(GL_DEPTH_TEST));
        //glCheck(glDepthMask(GL_FALSE));
        //glCheck(glEnable(GL_CULL_FACE));
        break;
    case Material::BlendMode::None:
        glCheck(glDisable(GL_BLEND));
        //glCheck(glEnable(GL_DEPTH_TEST));
        //glCheck(glDepthMask(GL_TRUE));
        //glCheck(glEnable(GL_CULL_FACE));
        break;
    }
}

void RenderSystem2D::onEntityAdded(Entity entity)
{
    //create the VBO (VAO is applied when shader is set)
    auto& drawable = entity.getComponent<Drawable2D>();
    CRO_ASSERT(drawable.m_vbo == 0, "This shouldn't be set yet!");
    glCheck(glGenBuffers(1, &drawable.m_vbo));

    //set up storage
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, drawable.m_vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
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