/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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
#include <crogine/ecs/Entity.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/MeshData.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <vector>

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
    also require a Model component created with a DynamicMeshBuilder that was initialised
    with VertexProperty flags for Position, Normal and Colour. The Model component also
    requires a single Material created with the built-in Billboard type shader.

    Creation of these models can be automated with the use of a ModelDefinition file.
    See the ModelDefinition class and model_definition_format.md document in the
    repository for further information

    Large BillboardCollections can be broken into smaller chunks and then callbacks
    applied, pre scene camera, to allow culling of chunks based on distance and visibility.

    First: call setChunkSize() with a non-zero number to set the size of each of the
    chunks in world units. Setting this to zero disables chunking and the collection
    is always drawn as a single mesh.

    By default the ModelRenderer will still draw all the chunks (in a single draw
    call) if there are no camera callbacks set. However using setDrawListCallback()
    you can apply a std::function, functor or bound function which is updated every
    frame via the CameraSystem's call to updateDrawLists().

    \begincode
    //example always draws chunk zero
    billboardCollection.setDrawListCallback(scene.getDefaultCamera(),
        [](cro::Entity camera, cro::Entity parent)
        {
            auto& bbc = parent.getComponent<cro::BillboardCollection>();
            //Chunks can otherwise be selected via frustum culling or distance to the camera
            const auto& chunk = bbc.getChunkIndices()[0];
            const auto& mesh = parent.getComponent<cro::Model>().getMeshData();

            auto& drawList = bbc.getDrawList(camera.getComponent<cro::Camera>().getDrawListIndex());
            drawList.clear();
            drawList.addChunk(mesh, chunk);
        };
    \endcode

    Calculations can be made using getChunkSize() getChunkCoordStart() and getChunkCount(). Also
    note that the default billboard shaders have uniforms u_nearFadeDistance and u_farFadeDistance
    which can be used in combination with culling to soften the effect.

    \see ModelDefinition
    */
    class CRO_EXPORT_API BillboardCollection final
    {
    public:
        /*!
        \brief Default constructor
        */
        BillboardCollection();


        /*!
        \brief Adds a Billboard to the collection.
        By default BillboardCollection components are empty. Using the Billboard
        struct to define geometry properties of the new Billboard, geometry is
        added by using this function.
        \param billboard A Billboard struct containing the properties of the billboard
        to add to the collection.
        */
        void addBillboard(Billboard billboard);


        /*!
        \brief Set the BillboardCollection with a pre-defined set of Billboards
        This overwrites any existing billboard data by *swapping* with the incoming
        data.
        */
        void setBillboards(std::vector<Billboard>& billboards);


        /*!
        \brief Set the size in world units of a chunk when rendering in multi-draw mode
        This defaults to 0 where all billboards are stored in one large single array.
        The maximum size of a chunk is 100 units.
        */
        void setChunkSize(std::uint32_t size);


        /*!
        \brief Sets a callback used by updateDrawlist() to optionally cull billboard chunks.
        \param camera The camera to which this callback is applied.
        \param callback A function to be called for this camera with the signature void(Entity camera, Entity parent)
        where camera is the assigned camera entity and parent is the entity to which this
        component is assigned.
        */
        void setDrawListCallback(Entity, const std::function<void(Entity, Entity)>& callback);


        /*!
        \brief This is the IBO index representing a single chunk
        */
        struct ChunkIndex final
        {
            std::uint32_t count = 0; //! < number of indices in the chunk
            std::uint32_t offset = 0; //! < offset IN BYTES into the IBO of the chunk (should be ADDED to IBOAllocation offset)
        };


        /*!
        \brief An array of offsets and index counts into the IBO
        This is created for each active camera as necessary and
        used by the ModelRenderer to decide which chunks of the
        BillboardCollection to draw.
        */
        struct DrawList final
        {
        public:
            /*!
            \brief Clears the underlying data
            */
            void clear()
            {
                indexSizeArray.clear();
                indexOffsetArray.clear();
            }

            /*!
            \brief Adds a given chunk to the given mesh
            */
            void addChunk(const Mesh::Data& mesh, const ChunkIndex& chunk)
            {
                indexOffsetArray.push_back(reinterpret_cast<void*>(mesh.indexData[0].iboAllocation.offset + chunk.offset));
                indexSizeArray.push_back(chunk.count);
            }

        private:
            friend class BillboardSystem;
            std::vector<GLsizei> indexSizeArray; //! < contains the sizes of each index buffer used in multidraw
            std::vector<void*> indexOffsetArray; //! < contains the pointers to offsets into the IBO in multidraw
        };



        /*!
        \brief Accessors provided for use in drawlist callbacks.
        */
        const std::vector<ChunkIndex>& getChunkIndices() const { return m_chunkIndices; }
        

        /*!
        \brief Total number of chunks in x and y coords
        */
        glm::uvec2 getChunkCount() const { return m_chunkCount; }


        /*!
        \brief Chunk size in world coords
        */
        float getChunkSize() const { return static_cast<float>(m_chunkSize); }


        /*!
        \brief Coordinates of the starting chunk.
        Note this is in LOCAL coordinates
        */
        glm::vec2 getChunkCoordStart() const { return m_chunkCoordStart; }


        /*!
        \brief Returns the drawlist for the given camera index.
        This is not automatically cleared and should be done by the
        callback if necessary.
        */
        DrawList& getDrawList(std::size_t cameraDrawListIndex)
        {
            assert(cameraDrawListIndex < m_drawLists.size());
            return m_drawLists[cameraDrawListIndex];
        }

    private:
        std::vector<Billboard> m_billboards;
        std::uint32_t m_chunkSize; //! < chunk size in world units
        bool m_dirty;


        std::vector<ChunkIndex> m_chunkIndices; //! < list of total available chunks

        glm::uvec2 m_chunkCount; //! < this times m_chunkSize grid
        glm::vec2 m_chunkCoordStart; //! < local coords of grid corner


        //drawlist for each available camera in the Scene
        //called from BillboardSystem::updateDrawlists()
        std::vector<DrawList> m_drawLists;

        using CameraCallback = std::function<void(Entity, Entity)>;

        //custom callbacks per camera for culling indices
        //params Camera entity, parent entity
        std::vector<CameraCallback> m_cameraCallbacks;


        //this is a bit of a fudge, but at the point in time at which
        //camera callbacks are most likely added cameras won't yet have
        //their drawlist IDs assigned, so we need to defer this until
        //the CameraSystem has been updated at least once...
        std::vector<std::pair<Entity, CameraCallback>> m_pendingCallbacks;

        friend class BillboardSystem;
    };
}