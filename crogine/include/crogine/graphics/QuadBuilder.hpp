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
        \param texRepeat Number of times to repeat the texture in the S and T
        axes. NOTE texture repeat should be applied to the textures used
        for the quad material when setting to a value greater than 1
        */
        QuadBuilder(glm::vec2 size, glm::vec2 texRepeat = glm::vec2(1.f, 1.f));

        int32 getUID() const override { return m_uid; }

    private:
        glm::vec2 m_size = glm::vec2(0.f);
        glm::vec2 m_repeat = glm::vec2(0.f);
        int32 m_uid;
        Mesh::Data build() const override;
    };
}