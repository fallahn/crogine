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
    offset (in bytes) into the buffer and number
    of blocks allocated. This must be used by
    the requestee (ie Drawable2D) when returning
    memory to the allocator so that it can be
    marked free
    */
    struct CRO_EXPORT_API BufferAllocation final
    {
        //offset in bytes into the buffer
        std::size_t offset = 0;
        //number of blocks allocation (0 if failed
        //of if the ID is a handle to a unique buffer)
        std::size_t blockCount = 0;
        //handle to the allocated buffer
        std::uint32_t bufferID = 0;
    };

    /*
    Creates a buffer and allocates blocks of N chunks to allow multiple
    items to share a single buffer. Can be inherited from for specific
    targets, mostly because I already implemented this as VBO specific
    and I really can't cope with renaming all the symbols *yet again*
    */
    class BufferAllocator : public SDLResource
#ifdef CRO_DEBUG_
        , public GuiClient
#endif
    {
    protected:
        /*!
        \brief Constructor
        \param blockSize Number of vertices per block
        \param dataSize Size of a data in bytes
        \param target Which kind of buffer is this? eg GL_ARRAY_BUFFER
        */
        explicit BufferAllocator(std::uint32_t blockSize, std::uint32_t vertexSize, std::uint32_t target);
        virtual ~BufferAllocator();

        BufferAllocator(const BufferAllocator&) = delete;
        BufferAllocator(BufferAllocator&&) = delete;
        BufferAllocator& operator = (const BufferAllocator&) = delete;
        BufferAllocator& operator = (BufferAllocator&&) = delete;

    public:
        /*!
        \brief Calculates the blocks required for the given number of chunks
        Rounds up to the nearest whole block.
        \param chunkCount The number of chunks for which blocks
        should be assigned.
        */
        std::size_t getBlockCount(std::size_t chunkCount) const;

        /*!
        \brief Requests an allocation from the current buffer for
        the given number of data chunks (eg: vertices).
        \param chunkCount The number of vertices for which
        buffer space is required.
        \returns An Allocation containing the buffer id and offset
        into the buffer in bytes. This should be kept for use
        when returning the memory to the buffer pool.
        */
        BufferAllocation newAllocation(std::size_t chunkCount);

        /*!
        \brief Frees a previously allocated chunk of buffer space.
        \param allocation A BufferAllocation struct containing the
        details of a previously allocated chunk of buffer space to
        return to the pool.
        */
        void freeAllocation(BufferAllocation allocation);

        //used for the debug display window
        void setDebugString(const std::string& s) { m_debugString = s; }

    private:
        //number of *data chunks* in a block
        const std::uint32_t m_blockSize;
        //size of data chunk in *bytes*
        const std::uint32_t m_dataSize;

        const std::uint32_t m_blockSizeBytes;

        const std::uint32_t m_target;

        std::uint32_t m_bufferID;

        //offset to the end of allocated space (in bytes)
        std::size_t m_finalOffset;

        //amount currently allocated to buffer - once we hit this we need to resize the buffer
        std::size_t m_bufferAllocationSize;

        struct FreeBlock final
        {
            //bytes from the beginning of the buffer to the start of the free block
            std::size_t offset = 0;
            //offset in blocks from the beginning
            std::size_t blockIndex = 0;
            //number of blocks which are free (block bytes size = m_dataSize * m_blockSize)
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
}

namespace cro
{
    namespace Detail
    {
        class VBOAllocator;
    }

    /*!
    \brief Collection of allocators which can be passed
    via a resource manager to a factory object such as MeshBuilder
    */
    class CRO_EXPORT_API AllocationResource final
    {
    public:
        Detail::VBOAllocator* getVBOAllocator(std::uint32_t blockSize, std::uint32_t vertexSize)
        {
            std::uint64_t uid = blockSize;
            uid <<= 32;
            uid |= vertexSize;

            if (m_vboAllocators.count(uid) == 0)
            {
                m_vboAllocators.insert(std::make_pair(uid, std::make_unique<Detail::VBOAllocator>(blockSize, vertexSize)));
            }
            return m_vboAllocators.at(uid).get();
        }

    private:
        //for some reason I can't use a unique_ptr??
        std::unordered_map<std::uint64_t, std::shared_ptr<Detail::VBOAllocator>> m_vboAllocators;
    };
}