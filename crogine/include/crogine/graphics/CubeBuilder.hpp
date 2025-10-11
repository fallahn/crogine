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
    \brief Creates a 1x1x1 cube mesh with texture coordinates
    */
    class CRO_EXPORT_API CubeBuilder final : public MeshBuilder
    {
    public:
        explicit CubeBuilder(glm::vec3 = glm::vec3(1.f));
        std::size_t getUID() const override { return m_uid; }
    private:
        std::size_t m_uid;
        glm::vec3 m_dimensions;
        Mesh::Data build(AllocationResource*) const override;
    };
}