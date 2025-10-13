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

#include <crogine/detail/BufferAllocation.hpp>

namespace cro::Detail
{
    struct IBOAllocation : public BufferAllocation
    {
        //used by glDrawElementsBaseVertex for index offset
        std::int32_t baseVertex = 0;

        IBOAllocation& operator = (const BufferAllocation& r)
        {
            blockCount = r.blockCount;
            bufferID = r.bufferID;
            offset = r.offset;
            return *this;
        }
    };

    class IBOAllocator final : public BufferAllocator
    {
    public:
        /*!
        \brief Constructor
        \param blocksSize Number of dataSize entries in a block, eg: 3 for triangles
        \param dataSize Size of the index data in bytes eg 2 for short ir 4 for int
        */
        explicit IBOAllocator(std::uint32_t blockSize, std::uint32_t dataSize);
    };
}
