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
    struct VBOAllocation final
    {
        std::size_t offset = 0;
        std::size_t blockCount = 0; //remains 0 if allocation failed for some reason
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
        /*
        \param blockSize Number of vertices per block
        \param vertexSize Size of a vertex in bytes
        */
        VBOAllocator(std::uint32_t blockSize, std::uint32_t vertexSize);
        ~VBOAllocator();

        VBOAllocator(const VBOAllocator&) = delete;
        VBOAllocator(VBOAllocator&&) = delete;
        VBOAllocator& operator = (const VBOAllocator&) = delete;
        VBOAllocator& operator = (VBOAllocator&&) = delete;

        //calculates the blocks required for the given numberof verts
        std::size_t getBlockCount(std::size_t vertexCount) const;

        VBOAllocation newAllocation(std::size_t vertexCount);
        void freeAllocation(VBOAllocation);

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

        //amount currently allocated to VBO - once we hit this re need to resize the buffer
        std::size_t m_vboAllocationSize;

        struct FreeBlock final
        {
            //bytes from the beginning of the VBO to the start of the free block
            std::size_t offset = 0;
            //number of blocks which are free (block bytes size = m_vertexSize * m_blockSize)
            std::size_t blockCount = 0;
            //total size in bytes of free blocks
            std::size_t totalSize = 0;

            bool eraseMe = false; //for remove_if
        };

        //free blocks which can be re-assigned. Ordered by
        //offset, starting nearest zero. If this is empty
        //blocks are allocated from the end of the VBO @
        //m_finalOffset
        std::vector<FreeBlock> m_freeBlocks;


        std::string m_debugString;
    };
}