/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/detail/glm/vec3.hpp>

#include <cctype>
#include <array>

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
        \brief used to map vertex attributes to shader input
        */
        enum Attribute
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

        /*!
        \brief Index data for sub-mesh
        */
        struct CRO_EXPORT_API IndexData final
        {
            uint32 ibo = 0;
            uint32 primitiveType = 0;
            uint32 indexCount = 0;
            uint32 format = 0;
            static const std::size_t MaxBuffers = 32;
        };

        /*!
        \brief Struct of mesh data used by Model components
        */
        struct CRO_EXPORT_API Data final
        {
            std::size_t vertexCount = 0;
            std::size_t vertexSize = 0;
            uint32 vbo = 0;
            uint32 primitiveType = 0;
            std::array<std::size_t, Mesh::Attribute::Total> attributes{}; //< size of attribute if it exists

            //index arrays
            std::size_t submeshCount = 0;
            std::array<Mesh::IndexData, Mesh::IndexData::MaxBuffers> indexData{};

            //spatial bounds
            Box boundingBox;
            Sphere boundingSphere;
        };
    }
}