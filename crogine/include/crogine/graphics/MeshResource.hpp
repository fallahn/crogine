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
#include <crogine/detail/SDLResource.hpp>
#include <crogine/detail/VBOAllocation.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

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
        \param forceReload If a mesh has already been loaded then this will forcefully
        delete its resources and reload the file. You MUST make sure any existing references
        to the mesh are not active in the scene, for instance by destroying any entities
        which have model using the mesh. Defaults to false.
        \returns Automatically generated ID, or 0 if loading failed.
        */
        std::size_t loadMesh(const MeshBuilder& mb, bool forceReload = false);

        /*!
        \brief Returns the mesh data for the given ID.
        */
        const Mesh::Data& getMesh(std::size_t ID) const;
        Mesh::Data& getMesh(std::size_t ID);

        /*!
        \brief Returns the skeletal animation for the mesh with the given ID.
        If the mesh contains no valid skeletal data then the default skeleton
        is returned.
        \param ID The mesh ID of the skeletal animation data to fetch.
        */
        Skeleton getSkeltalAnimation(std::size_t ID) const;

        /*!
        \brief Forcefully deletes all meshes
        Generally not needed as this is taken care of when the resource manager
        is destroyed, but if it is used make sure all entities which have a model
        components are first completely destroyed and removed from their Scene
        */
        void flush();

    private:
        AllocationResource m_allocationResource;
        
        std::unordered_map<std::size_t, Mesh::Data> m_meshData;
        std::unordered_map<std::size_t, Skeleton> m_skeletalData;

        void deleteMesh(Mesh::Data);
    };
}