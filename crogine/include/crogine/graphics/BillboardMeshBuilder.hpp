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

#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/MeshBuilder.hpp>

namespace cro
{
    /*!
    \brief Creates Billboard meshes with an optimised vertex layout.
    Used by the ModelDefinition class when creating billboard meshes
    from and asset file loaded from disk.
    */
    class CRO_EXPORT_API BillboardMeshBuilder final : public cro::MeshBuilder
    {
    public:
        /*!
        \brief Constructor
        */
        BillboardMeshBuilder();

        /*!
        \brief UID used by MeshResource class.
        This always returns 0 which means each instance of a mesh created with this builder is unique.
        */
        std::size_t getUID() const override { return 0; }


        /*!
        \brief Vertex layout description
        */
        struct VertexLayout final
        {
            glm::vec3 pos = glm::vec3(0.f);
            Detail::ColourLowP colour;
            glm::vec3 rootPos = glm::vec3(0.f);
            std::uint32_t uvCoords = 0; //packed with glm::packSnorm2x16()
            std::uint32_t size = 0; //packed into GL_HALF_FLOAT with glm::packHalf2x16();
        };

    private:


        Mesh::Data build() const override;
    };
}