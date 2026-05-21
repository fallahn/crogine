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
#include <crogine/graphics/Rectangle.hpp>

#include <crogine/detail/glm/vec2.hpp>

namespace cro
{
    /*!
    \brief Creates a texturable quad mesh, with the origin in the centre
    */
    class CRO_EXPORT_API QuadBuilder final : public MeshBuilder
    {
    public:
        /*!
        \brief Constructor.
        \param size Dimensions of the quad created by this mesh builder
        \param textureRect Normalised UV coordinates given as a cro::FloatRect
        */
        QuadBuilder(glm::vec2 size, FloatRect textureRect = {glm::vec2(0.f), glm::vec2(1.f)});

        std::size_t getUID() const override { return m_uid; }

    private:
        glm::vec2 m_size = glm::vec2(0.f);
        FloatRect m_textureRect;
        std::size_t m_uid;
        Mesh::Data build(AllocationResource*) const override;
    };
}