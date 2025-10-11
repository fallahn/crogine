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
    \brief Creates a mesh with an empty vertex buffer.
    Useful for instances that require dynamically updated vertex data,
    the DynamicMeshBuilder returns a mesh whose buffer is empty, but with
    a vertex layout created from the given flags. For example a dynamic
    mesh could be used to batch sprites or billboards that are regularly
    updated, but still need to be rendered with the ModelRenderer/RenderSystem3D.

    The vertex layout can be requested by passing a bitfield of flags to the
    constructor. The DynamicMeshBuilder then creates a vertex buffer to
    match those requirements with the GL_DYNAMIC_DRAW flag set. The flags
    are defined in the VertexProperty enum.

    Binding and updating the VBO with vertex data (and correctly updating the
    AABB) is then left to the user/custom Systems, including ensuring the
    data given correctly matches the requested layout.

    Buffers are also drawn using index arrays so at least 1 sub-mesh needs to
    be requested and index data updated with the accompanying vertex data.
    Index arrays use unsigned integer format.

    Remember when updating the vertex buffer and index arrays to also
    update the Mesh::Data with the new vertex count and new index count.

    */

    class CRO_EXPORT_API DynamicMeshBuilder final : public cro::MeshBuilder
    {
    public:
        /*!
        \brief Constructor
        \param flags A bitfield of VertexProperty flags which define the vertex attributes of the created buffer
        \param submeshCount At least one sub-mesh should be requested
        \param primitiveType OpenGL primitive type to use when drawing, eg GL_TRIANGLE_STRIP
        */
        DynamicMeshBuilder(std::uint32_t flags, std::uint8_t submeshCount, std::uint32_t primitiveType);

        /*!
        \brief UID used by MeshResource class.
        This always returns 0 which means each instance of a mesh created with this builder is unique.
        */
        std::size_t getUID() const override { return 0; }
    private:

        std::uint32_t m_flags;
        std::uint8_t m_submeshCount;
        std::uint32_t m_primitiveType;

        Mesh::Data build(AllocationResource*) const override;
    };
}