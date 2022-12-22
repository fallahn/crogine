/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/graphics/Transformable2D.hpp>
#include <crogine/graphics/SimpleDrawable.hpp>

namespace cro
{
    class Texture;

    /*
    \brief Simple Vertex Array drawable.
    Draws a given array of cro::Vertex2D to the active target, in target coordinates.
    SimpleVertexArrays are non-copyable.
    */
    class CRO_EXPORT_API SimpleVertexArray final : public Transformable2D, public SimpleDrawable
    {
    public:
        /*!
        \brief Default constructor
        */
        SimpleVertexArray();

        /*!
        \brief Constructor
        \param texture A valid texture with which to draw the array.
        */
        explicit SimpleVertexArray(const Texture& texture);

        /*!
        \brief Sets a new texture for the array.
        \param texture The new texture to use. UV coordinates are set
        via the vertex data.
        */
        void setTexture(const Texture& texture);

        /*!
        \brief Sets the OpenGL primitive type with which to
        render the vertex data
        Must be GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN
        GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, or GL_POINTS
        Defaults to GL_TRIANGLE_STRIP
        */
        void setPrimitiveType(std::uint32_t primitiveType);

        /*!
        \brief Sets the array of vertex data to draw
        \see Vertex2D
        */
        void setVertexData(const std::vector<cro::Vertex2D>& data);

        /*!
        \brief Draws the array to the active target
        \param parentTransform An optional transform which
        is multiplied by the VertexArray's transform to allow
        creating scene-graph like hierarchies.
        */
        void draw(const glm::mat4& parentTransform = glm::mat4(1.f)) override;

    private:


    };
}