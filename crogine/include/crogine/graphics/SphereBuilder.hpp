/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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
    \brief Creates a spherical mesh with cube-mapped texture coordinates.
    */
    class CRO_EXPORT_API SphereBuilder final : public MeshBuilder
    {
    public:
        /*!
        \brief Constructor
        \param radius Radius of sphere to create, defaults to 0.5
        \param resolution Number of quads per side. Defaults to 4 which
        is 4*4 per side, 6 sides = 96 quads for 192 triangles. Higher
        numbers result in a smoother mesh but increased triangle count.
        */
        SphereBuilder(float radius = 0.5f, std::size_t resolution = 4);

        std::size_t getUID() const override { return m_uid; }

    private:
        float m_radius;
        std::size_t m_resolution;
        std::size_t m_uid;
        Mesh::Data build(AllocationResource*) const override;
    };
}