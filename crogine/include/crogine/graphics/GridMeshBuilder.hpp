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

#pragma once

#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <crogine/detail/glm/vec2.hpp>

namespace cro
{
    /*!
    \brief Creates a texturable grid mesh formed of a triangle strip
    UV coordinates are mapped from 0,0 at the bottom left to 1,1 at the top right
    */
    class CRO_EXPORT_API GridMeshBuilder final : public MeshBuilder
    {
    public:
        /*!
        \brief Constructor.
        \param size Dimensions of the quad created by this mesh builder
        \param subDivisions Number of sub-divisions along each side of the grid
        */
        GridMeshBuilder(glm::vec2 size, std::size_t subDivisions);

        std::size_t getUID() const override { return 0; }

    private:
        glm::vec2 m_size = glm::vec2(0.f);
        std::size_t m_subDivisions;
        Mesh::Data build() const override;
    };
}