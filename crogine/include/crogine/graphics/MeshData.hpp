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

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/Spatial.hpp>

#include <crogine/detail/VBOAllocation.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <cctype>
#include <array>
#include <vector>

namespace cro
{
    namespace Mesh
    {
        /*!
        \brief Mesh Identifiers.
        These are default mappings for primitive shapes. Extend
        these by starting from Count and map them to mesh buffers
        by using the MeshResource load function.
        */
        enum ID
        {
            QuadMesh = 0,
            CubeMesh,
            SphereMesh,
            Count
        };


        /*!
        \brief Index data for sub-mesh
        */
        struct CRO_EXPORT_API IndexData final
        {
            std::uint32_t ibo = 0;
            enum Pass
            {
                Final, Shadow, Count
            };

            //the VAOs here are only included for convenience when rendering
            //custom meshes (such as the BSP renderer) and are not used by
            //Model components when rendering. They are managed by the 
            //MeshResource however - and will be deleted when the IBO/VBO is
            //deleted for a given Mesh::Data struct. For this reason care should
            //be taken for lifetime management when copying this data...
            std::array<std::uint32_t, Pass::Count> vao = {0,0};
            std::uint32_t primitiveType = 0;
            std::uint32_t indexCount = 0;
            std::uint32_t format = 0;
            static const std::size_t MaxBuffers = 32;
        };

        /*!
        \brief Contains the size and type of a vertex attribute if it exists.
        */
        struct CRO_EXPORT_API Attribute final
        {
            std::uint32_t size = 0; //number of components
            std::uint32_t glType = 0x1406; //GL_FLOAT;
            std::uint32_t glNormalised = 0; // GL_FALSE;

            enum
            {
                Invalid = -1,
                Position = 0,
                Colour,
                Normal,
                Tangent,
                Bitangent,
                UV0,
                UV1,
                BlendIndices,
                BlendWeights,

                Total
            };
        };

        /*!
        \brief Struct of mesh data used by Model components
        */
        struct CRO_EXPORT_API Data final
        {
            //it's up to a specific MeshBuilder implementation
            //to use this - 0 block allocations have a unique
            //VBO (if the ID is valid) rather than a shared one
            Detail::VBOAllocation vboAllocation;
            std::size_t vertexCount = 0;
            std::size_t vertexSize = 0; //!< size of a single vertex *in bytes*
            std::uint32_t primitiveType = 0;
            std::array<Attribute, Attribute::Total> attributes{}; //!< size of attribute if it exists
            std::uint32_t attributeFlags = 0; //!< bitmask of VertexProperty flags indicating the current properties of the vertex data.

            //index arrays
            std::size_t submeshCount = 0;
            std::array<IndexData, IndexData::MaxBuffers> indexData{};

            //spatial bounds
            Box boundingBox;
            Sphere boundingSphere;
        };

        /*!
        \brief Utility to read back vertex data and index data
        */
        void CRO_EXPORT_API readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint8_t>>& destIndices);
        void CRO_EXPORT_API readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint16_t>>& destIndices);
        void CRO_EXPORT_API readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint32_t>>& destIndices);
    }
}