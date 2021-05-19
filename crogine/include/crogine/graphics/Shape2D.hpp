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

#pragma once

#include <crogine/Config.hpp>

#include <crogine/graphics/Vertex2D.hpp>

#include <vector>

namespace cro
{
    class CRO_EXPORT_API Shape
    {
    public:
        /*!
        \brief Utility function for creating 2D rectangle shapes.
        This function will return a vector of Vertex2D which can be used with
        Drawable2D::setVertexData(). The returned vertices create a 2D outline
        of a rectangle and should be drawn with GL_LINE_STRIP as the primitive type.
        \param size The width and height of the rectangle to create
        \param colour The initial colour of the vertices
        \returns std::vector<Vertex2D> The return value should be used with a
        Drawable2D component, via setVertexData()
        \see Drawable2D
        */
        static std::vector<Vertex2D> rectangle(glm::vec2 size, cro::Colour colour);


        /*!
        \brief Utility function for creating 2D circle shapes.
        This function will return a vector of Vertex2D which can be used with
        Drawable2D::setVertexData(). The returned vertices create a 2D outline
        of a circle and should be drawn with GL_LINE_STRIP as the primitive type.
        Note that by reducing the point count other regular polygons such as
        triangles and hexagons can easily be created.
        \param radius The radius of the shape to create.
        \param colour The initial colour of the vertices
        \param pointCount The number of points used to make up the circle. Defaults to 12
        \returns std::vector<Vertex2D> The return value should be used with a
        Drawable2D component, via setVertexData()
        \see Drawable2D
        */
        static std::vector<Vertex2D> circle(float radius, cro::Colour colour, std::size_t pointCoint = 12);
    };
}