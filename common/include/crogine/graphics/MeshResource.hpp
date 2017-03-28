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

#ifndef CRO_MESH_RESOURCE_HPP_
#define CRO_MESH_RESOURCE_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <unordered_map>
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
            Quad = 0,
            Cube,
            Sphere,
            Count
        };

        /*!
        \brief used to map vertex attributes to shader input
        */
        enum Attribute
        {
            Invalid,
            Position = 0,
            Colour,
            Normal,
            Tangent,
            Bitangent,
            UV0,
            UV1,
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
            uint32 vbo = 0;
            uint32 primitiveType = 0;
            std::array<std::size_t, Mesh::Attribute::Total> attributes{};

            //index arrays
            std::size_t submeshCount = 0;
            std::array<Mesh::IndexData, Mesh::IndexData::MaxBuffers> indexData{};
        };
    }
    
    /*!
    \brief Resource for mesh data.
    The mesh resource handles loading and unloading buffer data. It
    also creates the mesh data structs required by model components.
    The mesh resource needs to live at least as long as the Scene
    which uses the meshes curated by it.
    */
    class CRO_EXPORT_API MeshResource final : public Detail::SDLResource
    {
    public:       

        MeshResource();
        ~MeshResource();

        MeshResource(const MeshResource&) = delete;
        MeshResource(const MeshResource&&) = delete;
        MeshResource& operator = (const MeshResource&) = delete;
        MeshResource& operator = (const MeshResource&&) = delete;

        /*!
        \brief Preloads mesh assets and maps them to the given ID.
        IDs should start at Mesh::ID::Count as other Mesh::ID values
        are used for preloading the mesh primitive types.
        */
        bool loadMesh(/*MeshParser,*/ int32 ID);

        /*!
        \brief Returns the mesh data for the given ID.
        */
        const Mesh::Data getMesh(int32 ID) const;

    private:
        std::unordered_map<int32, Mesh::Data> m_meshData;

        void loadQuad();
        void loadCube();
        void loadSphere();
    };
}


#endif //CRO_MESH_RESOURCE_HPP_