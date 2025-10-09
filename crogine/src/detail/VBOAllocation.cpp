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
    m_blockSizeBytes(blockSize * vertexSize),
    m_vbo           (0),
    m_finalOffset   (0)
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
                ImGui::Text("Allocated VBO Size: %d bytes (%d blocks)", m_finalOffset, m_finalOffset / m_blockSizeBytes);

                std::vector<bool> blockCells(m_finalOffset / m_blockSizeBytes);
                std::fill(blockCells.begin(), blockCells.end(), true);

                if (m_freeBlocks.empty())
                {
                    ImGui::Text("No free blocks");
                }
                else
                {
                    std::size_t blockCount = 0;
                    for (const auto& fb : m_freeBlocks)
                    {
                        blockCount += fb.blockCount;

                        const auto index = fb.offset / m_blockSizeBytes;
                        for (auto i = index; i < index + fb.blockCount; ++i)
                        {
                            blockCells[i] = false;
                        }
                    }
                    ImGui::Text("Free blocks: %d", blockCount);
                }

                int i = 0;
                for (const auto& b : blockCells)
                {
                    if (b)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, { 1.f, 1.f, 0.f, 1.f });
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, { 1.f, 0.f, 0.f, 1.f });
                    }

                    const std::string label = "##" + std::to_string(i++);
                    ImGui::Button(label.c_str(), { 10.f, 14.f });
                    ImGui::PopStyleColor();

                    if (i % 32 != 0)
                    {
                        ImGui::SameLine();
                    }
                }

                ImGui::NewLine();
                for (const auto& fb : m_freeBlocks)
                {
                    ImGui::Text("Free block at %d, size %d blocks", fb.offset, fb.blockCount);
                }
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

    VBOAllocation ret;
    ret.blockCount = blocks;
    ret.vboID = m_vbo;

    if (m_freeBlocks.empty())
    {
        //allocate at the end
        ret.offset = m_finalOffset;
        m_finalOffset += blocks * m_blockSizeBytes;
    }
    else
    {
        std::size_t removeIndex = std::numeric_limits<std::size_t>::max();

        //reuse the first free block which fits
        for (auto i = 0u; i < m_freeBlocks.size(); ++i)
        {
            if (m_freeBlocks[i].blockCount >= blocks)
            {
                //we fit here
                ret.offset = m_freeBlocks[i].offset;

                m_freeBlocks[i].blockCount -= blocks;
                m_freeBlocks[i].offset += blocks * m_blockSizeBytes;

                removeIndex = i;
                break;
            }
        }

        if (removeIndex == std::numeric_limits<std::size_t>::max())
        {
            //we didn't find space to allocate at the end
            ret.offset = m_finalOffset;
            m_finalOffset += blocks * m_blockSizeBytes;
        }
        else
        {
            //erase the FreeBlock from the list if the block count is now 0
            if (m_freeBlocks[removeIndex].blockCount == 0)
            {
                m_freeBlocks.erase(m_freeBlocks.begin() + removeIndex);
            }
        }
    }

    LogI << "Allocated " << blocks << " VBO blocks at: " << ret.offset << std::endl;
    return ret;
}

void VBOAllocator::freeAllocation(VBOAllocation allocation)
{
    //TODO make sure we're not somehow double freeing (probably wants to throw in which case)
    if (allocation.vboID == m_vbo
        && allocation.blockCount != 0)
    {
        auto& fb = m_freeBlocks.emplace_back();
        fb.blockCount = allocation.blockCount;
        fb.offset = allocation.offset;
        fb.totalSize = allocation.blockCount * m_blockSizeBytes;

        std::sort(m_freeBlocks.begin(), m_freeBlocks.end(), 
            [](const FreeBlock& a, const FreeBlock& b) {return a.offset < b.offset; });

        if (m_freeBlocks.size() > 1)
        {
            //then merge any contiguous blocks
            for (auto i = m_freeBlocks.size()-1; i > 0; --i)
            {
                if (m_freeBlocks[i].offset ==
                    m_freeBlocks[i - 1].offset + m_freeBlocks[i - 1].totalSize)
                {
                    LogI << "Merging..." << std::endl;
                    m_freeBlocks[i - 1].blockCount += m_freeBlocks[i].blockCount;
                    m_freeBlocks[i - 1].totalSize = m_freeBlocks[i - 1].blockCount * m_blockSizeBytes;

                    m_freeBlocks[i].eraseMe = true;
                }
            }

            m_freeBlocks.erase(std::remove_if(m_freeBlocks.begin(), m_freeBlocks.end(),
                [](const FreeBlock& fb)
                {
                    return fb.eraseMe;
                }),
                m_freeBlocks.end());
        }
        LogI << "Freed " << allocation.blockCount << " VBO blocks at: " << allocation.offset << std::endl;
    }
}