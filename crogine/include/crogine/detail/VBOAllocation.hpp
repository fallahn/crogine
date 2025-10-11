/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include <crogine/detail/SDLResource.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/GuiClient.hpp>
#endif

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace cro::Detail
{
    /*
    Returned after allocation, contains the
    offset (in bytes) into the VBO and number
    of blocks allocated. This must be used by
    the requestee (ie Drawable2D) when returning
    memory to the allocator so that it can be
    marked free
    */
    struct CRO_EXPORT_API VBOAllocation final
    {
        //offset in bytes into the VBO
        std::size_t offset = 0;
        //number of blocks allocation (0 if failed
        //of if the vboID is a handle to a unique VBO)
        std::size_t blockCount = 0;
        //handle to the allocated VBO
        std::uint32_t vboID = 0;
    };

    /*
    Creates a VBO and allocates blocks of N vertex chunks to allow multiple
    items to share a single VBO. For example:
    Drawable2D components request blocks of 4 vertices at a time, each vertex
    sized to Vertex2D.
    */
    class VBOAllocator final : public SDLResource
#ifdef CRO_DEBUG_
        , public GuiClient
#endif
    {
    public: 
        /*!
        \brief Constructor
        \param blockSize Number of vertices per block
        \param vertexSize Size of a vertex in bytes
        */
        explicit VBOAllocator(std::uint32_t blockSize, std::uint32_t vertexSize);
        ~VBOAllocator();

        VBOAllocator(const VBOAllocator&) = delete;
        VBOAllocator(VBOAllocator&&) = delete;
        VBOAllocator& operator = (const VBOAllocator&) = delete;
        VBOAllocator& operator = (VBOAllocator&&) = delete;

        /*!
        \brief Calculates the blocks required for the given number of verts
        Rounds up to the nearest whole block.
        \param vertexCount The number of vertices for which blocks
        should be assigned.
        */
        std::size_t getBlockCount(std::size_t vertexCount) const;

        /*!
        \brief Requests an allocation from the current VBO for
        the given number of vertices.
        \param vertexCount The number of vertices for which
        VBO space is required.
        \returns An Allocation containingthe VBO id and offset
        into the buffer in bytes. This should be kept for use
        when returning the memory to the VBO pool.
        */
        VBOAllocation newAllocation(std::size_t vertexCount);

        /*!
        \brief Frees a previously allocated chunk of VBO space.
        \param allocation A VBOAllocation struct containing the
        details of a previously allocated chunk of VBO space to
        return to the pool.
        */
        void freeAllocation(VBOAllocation allocation);

        //used for the debug display window
        void setDebugString(const std::string& s) { m_debugString = s; }

    private:
        //number of *verts* in a block
        const std::uint32_t m_blockSize;
        //size of vertex in *bytes*
        const std::uint32_t m_vertexSize;

        const std::uint32_t m_blockSizeBytes;

        std::uint32_t m_vbo;

        //offset to the end of allocated space (in bytes)
        std::size_t m_finalOffset;

        //amount currently allocated to VBO - once we hit this we need to resize the buffer
        std::size_t m_vboAllocationSize;

        struct FreeBlock final
        {
            //bytes from the beginning of the VBO to the start of the free block
            std::size_t offset = 0;
            //offset in blocks from the beginning
            std::size_t blockIndex = 0;
            //number of blocks which are free (block bytes size = m_vertexSize * m_blockSize)
            std::size_t blockCount = 0;

            bool eraseMe = false; //for remove_if
        };

        //free blocks which can be re-assigned. Ordered by
        //offset, starting nearest zero. If this is empty
        //blocks are allocated from the end of the VBO at
        //m_finalOffset
        std::vector<FreeBlock> m_freeBlocks;


        std::string m_debugString;
    };

    /*!
    \brief Recycle VAOs when needed rather than delete/create
    */
    class VAOAllocator final
    {
    public:
        /*!
        \brief Constructor.
        \param initialPoolSize Number of VAOs to create on construction.
        If more VAOs are requested then they are created on the fly if
        none are available in the pool
        */
        explicit VAOAllocator(std::size_t initialPoolSize = 0);
        ~VAOAllocator();

        VAOAllocator(const VAOAllocator&) = delete;
        VAOAllocator(VAOAllocator&&) = delete;

        VAOAllocator& operator = (const VAOAllocator&) = delete;
        VAOAllocator& operator = (VAOAllocator&&) = delete;

        /*!
        \brief Requests a VAO form the pool, creating a new one
        if necessary.
        \returns Handle to the VAO.
        */
        std::uint32_t requestVAO();

        /*!
        \brief Returns the given VAO to the pool
        \param vao The handle to return. Once returned
        any VAOs with this handle should be considered
        invalid, as it will eventually be recycled.
        */
        void freeVAO(std::uint32_t vao);

    private:

        //handles which are assigned and need deleting on shutdown
        std::vector<std::uint32_t> m_activeVAOs;

        //handles which have been returned and can be reused
        std::vector<std::uint32_t> m_freeVAOs;
    };
}

namespace cro
{
    /*!
    \brief Collection of allocators which can be passed
    via a resource manager to a factory object such as MeshBuilder
    */
    class CRO_EXPORT_API AllocationResource final
    {
    public:
        Detail::VBOAllocator* getAllocator(std::uint32_t blockSize, std::uint32_t vertexSize)
        {
            std::uint64_t uid = blockSize;
            uid <<= 32;
            uid |= vertexSize;

            if (m_allocators.count(uid) == 0)
            {
                m_allocators.insert(std::make_pair(uid, std::make_unique<Detail::VBOAllocator>(blockSize, vertexSize)));
            }
            return m_allocators.at(uid).get();
        }

    private:
        //for some reason I can't use a unique_ptr??
        std::unordered_map<std::uint64_t, std::shared_ptr<Detail::VBOAllocator>> m_allocators;
    };
}