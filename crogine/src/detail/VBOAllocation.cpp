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

#include "GLCheck.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/detail/VBOAllocation.hpp>

#include <crogine/gui/Gui.hpp>

using namespace cro::Detail;

VBOAllocator::VBOAllocator(std::uint32_t blockSize, std::uint32_t vertexSize)
    : m_blockSize   (blockSize),
    m_vertexSize    (vertexSize),
    m_vbo           (0)
{
    CRO_ASSERT(blockSize != 0, "");
    CRO_ASSERT(vertexSize != 0, "");

    glCheck(glGenBuffers(1, &m_vbo));

    if (!m_vbo)
    {
        LogE << "Failed to create VBO for allocation" << std::endl;
    }
#ifdef CRO_DEBUG_
    registerWindow(
        [&]()
        {
            if (ImGui::Begin("VBO Allocation"))
            {

            }
            ImGui::End();
        });

#endif
}

VBOAllocator::~VBOAllocator()
{
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }
}

//public
std::size_t VBOAllocator::getBlockCount(std::size_t vertexCount) const
{
    auto blocks = vertexCount / m_blockSize;
    if (vertexCount % m_blockSize)
    {
        blocks++;
    }
    return blocks;
}

VBOAllocation VBOAllocator::newAllocation(std::size_t vertexCount)
{
    const auto blocks = getBlockCount(vertexCount);

    //TODO search for the first offset which has enough free blocks to allocate

    VBOAllocation ret;
    ret.blockCount = blocks;
    ret.vboID = m_vbo;

    LogI << "Allocated " << blocks << " VBO blocks at: " << std::endl;
    return ret;
}

void VBOAllocator::freeAllocation(VBOAllocation allocation)
{
    //TODO make sure we're not somehow double freeing (probably wants to throw in which case)
    if (allocation.vboID == m_vbo
        && allocation.blockCount != 0)
    {


        LogI << "Freed " << allocation.blockCount << " VBO blocks at: " << allocation.offset << std::endl;
    }
}