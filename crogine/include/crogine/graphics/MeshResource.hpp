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
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/MeshData.hpp>

#include <unordered_map>
#include <array>

namespace cro
{    
    class MeshBuilder;

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
        \param ID Integer ID to map to the mesh created by the MeshBuilder instance
        \param mb Instance of a concrete MeshBuilder type to load / create
        a mesh to add to the resource       
        \returns true if mesh was successfully added to the resource holder
        */
        bool loadMesh(std::size_t ID, const MeshBuilder& mb);

        /*!
        \brief Preloads a mesh and automatically assigns an ID.
        \param mb Reference to the MeshBuilder instance containing the data to preload.
        \returns Automatically generated ID, or -1 if loading failed.
        */
        std::size_t loadMesh(const MeshBuilder& mb);

        /*!
        \brief Returns the mesh data for the given ID.
        */
        const Mesh::Data getMesh(std::size_t ID) const;

    private:
        std::unordered_map<std::size_t, Mesh::Data> m_meshData;
    };
}