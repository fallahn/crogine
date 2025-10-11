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

namespace cro
{
    /*!
    \brief Creates a texturable circular mesh formed of a triangle strip.
    The vertices are positioned around the centre of the circle
    */
    class CRO_EXPORT_API CircleMeshBuilder final : public MeshBuilder
    {
    public:
        /*!
        \brief Constructor.
        \param radius The radius of the circle to create. Must be greater than 0
        \param pointCount Number of points which make up the perimeter. Must be
        3 or more.
        */
        CircleMeshBuilder(float radius, std::uint32_t pointCount);

        std::size_t getUID() const override { return 0; }

    private:
        float m_radius;
        std::uint32_t m_pointCount;
        Mesh::Data build(AllocationResource*) const override;
    };
}