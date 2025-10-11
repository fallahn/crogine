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
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

#include <vector>
#include <array>


namespace cro::VertexProperty
{
    enum
    {
        Position     = (1 << 0),
        Colour       = (1 << 1),
        Normal       = (1 << 2),
        Tangent      = (1 << 3),
        Bitangent    = (1 << 4),
        UV0          = (1 << 5),
        UV1          = (1 << 6),
        BlendIndices = (1 << 7),
        BlendWeights = (1 << 8)
    };
}

namespace cro
{
    /*!
    \brief Base class for mesh creation.
    This class should be used as a base for mesh file format parsers
    or programmatically creating meshes. See the Quad, Cube or Sphere
    builders for examples.

    Mesh builder types should be used to pre-load mesh data with the
    MeshResource class. Mesh data created with a MeshBuilder instance
    will, on the surface, work with the Model component, but will not be 
    properly memory managed unless it is assigned a valid ID via the
    MeshResource manager, and lead to memory leaks.

    All meshes are drawn with at least one index array so this should
    be considered if creating a builder which dynamically builds meshes.
    \see DynamicMeshBuilder
    */
    class CRO_EXPORT_API MeshBuilder : public Detail::SDLResource
    {
    public:
        MeshBuilder() = default;
        virtual ~MeshBuilder() = default;
        MeshBuilder(const MeshBuilder&) = default;
        MeshBuilder(MeshBuilder&&) = default;
        MeshBuilder& operator = (const MeshBuilder&) = default;
        MeshBuilder& operator = (MeshBuilder&&) = default;

        /*!
        \brief If creating a builder which loads a mesh from disk, for example,
        create a Unique ID based on the hash of the file path to prevent the
        same mesh being loaded more than once if using the ResourceAutomation.

        Otherwise returning 0 will ensure each request from the mesh resource
        returns a unique instance of the mesh data.
        */
        virtual std::size_t getUID() const { return 0; }

        /*!
        \brief If the derived class is used to parse a format which contains
        skeletal animation data override this to return the base pose skeleton.
        These will then be cached by the mesh resource when the mesh is built
        and applied to any entities created via the ModelDefinition class.
        */
        virtual Skeleton getSkeleton() const { return {}; }

    protected:
        friend class MeshResource;
        friend class SpriteSystem3D;
        /*!
        \brief Implement this to create the appropriate VBO / IBO
        and return a Mesh::Data struct. Automatically called by the
        MeshResource class
        */
        virtual Mesh::Data build() const = 0;

        //returns the total number of components in a given set of attributes
        static std::size_t getAttributeSize(const std::array<Mesh::Attribute, Mesh::Attribute::Total>& attrib);
        //returns the size of the attribute layout in bytes
        static std::size_t getVertexSize(const std::array<Mesh::Attribute, Mesh::Attribute::Total>& attrib);
        static void createVBO(Mesh::Data& meshData, const std::vector<float>& vertexData);
        static void createIBO(Mesh::Data& meshData, const void* idxData, std::size_t idx, std::int32_t dataSize);
    };
}