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

#include "../../detail/glad.hpp"
#include "../../detail/GLCheck.hpp"

#include <string>

namespace
{
    const std::string VertexShader = 
        R"(
            uniform mat4 u_worldViewMatrix;
            uniform mat4 u_projectionMatrix;

            ATTRIBUTE vec4 a_position;
            ATTRIBUTE MED vec2 a_texCoord0;
            ATTRIBUTE LOW vec4 a_colour;

            VARYING_OUT LOW vec4 v_colour;

            #if defined(TEXTURED)
            VARYING_OUT MED vec2 v_texCoord;
            #endif

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;
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

        //check data flag and update buffer if needed
        if (drawable.m_updateBufferData)
        {

            drawable.m_updateBufferData = false;
        }

        //check shader flag and set correct flag if needed
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
    //TODO we need to pass in a reference to the active render
    //target so we can measure its size and multiply it by the
    //camera's active viewport

    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        const auto& drawable = entity.getComponent<Drawable2D>();
        const auto& transform = entity.getComponent<cro::Transform>();

        //check local bounds for visibility and draw if visible
        auto bounds = drawable.m_localBounds.transform(transform.getWorldTransform());
    
    
    
    }
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

        break;
    case Material::BlendMode::Alpha:

        break;
    case Material::BlendMode::Multiply:

        break;
    case Material::BlendMode::None:

        break;
    }
}

void RenderSystem2D::onEntityAdded(Entity entity)
{
    //create the VBO (and VAO if desktop)
}

void RenderSystem2D::onEntityRemoved(Entity entity)
{
    //remove any OpenGL buffers
}