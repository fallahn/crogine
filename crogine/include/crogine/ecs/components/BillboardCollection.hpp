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

#include <crogine/Config.hpp>
#include <crogine/graphics/Colour.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

namespace cro
{
    /*!
    \brief Billboard description properties.
    This struct is used to describe the properties of a Billboard quad when
    adding a new billboard to the collection.
    */
    struct CRO_EXPORT_API Billboard final
    {
        glm::vec2 size = glm::vec2(1.f); //!< the width and height of the quad
        glm::vec3 position = glm::vec3(0.f); //!< position of the quad relative to the entity's Transform
        glm::vec2 origin = glm::vec2(0.f); //!< the origin around which the quad geometry is centred
        Colour colour = Colour::White; //!< the colour of the billboard, multiplied by the material colour
        FloatRect textureRect = FloatRect(0.f,0.f,1.f,1.f); //!< subrect of the material texture to use, in texture coordinates
    };

    /*!
    \brief A collection of billboard quads rendered with a single mesh

    Billboarded quads are geometry which always face the camera. This is useful
    for grass or foliage effects where many instances of low detail are needed.

    The BillboardCollection components defines the properties of the billboards
    associated with a single model/mesh. As such entities with a BillboardCollection
    also require a Model component created with a DynamicMeshBuilder that was initialied
    with VertexProperty flags for Position, Normal and Colour. The Model component also
    requires a single Material created with the built-in Billboard type shader.

    Creation of these models can be automated with the use of a ModelDefinition file.
    See the ModelDefinition class and model_definition_format.md document in the
    repository for further information
    \see ModelDefinition
    */
    class CRO_EXPORT_API BillboardCollection final
    {
    public:
        /*!
        \brief Default constructor
        */
        BillboardCollection() = default;

        /*!
        \brief Adds a Billboard to the collection.
        By default BillboardCollection components are empty. Using the Billboard
        struct to define geometry properties of the new Billboard, geometry is
        added by using this function.
        \param billboard A Billboard struct containing the properties of the billboard
        to add to the collection.
        */
        void addBillboard(Billboard billboard)
        {
            m_billboards.push_back(billboard);
            m_dirty = true;
        }

    private:
        bool m_dirty = false;
        std::vector<Billboard> m_billboards;

        friend class BillboardSystem;
    };
}