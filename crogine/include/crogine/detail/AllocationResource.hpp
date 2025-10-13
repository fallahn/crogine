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

#include <crogine/detail/IBOAllocation.hpp>
#include <crogine/detail/VBOAllocation.hpp>

#include <memory>
#include <unordered_map>

namespace cro
{
    /*!
    \brief Collection of allocators which can be passed
    via a resource manager to a factory object such as MeshBuilder
    */
    class CRO_EXPORT_API AllocationResource final
    {
        //TODO feels like we ought to be able to template this somehow...
        //perhaps if we can generate UID for concrete type we can pointer to base class?
    public:
        Detail::IBOAllocator* getIBOAllocator(std::uint32_t blockSize, std::uint32_t dataSize)
        {
            const auto uid = getUID(blockSize, dataSize);

            if (m_iboAllocators.count(uid) == 0)
            {
                m_iboAllocators.insert(std::make_pair(uid, std::make_unique<Detail::IBOAllocator>(blockSize, dataSize)));
            }
            return m_iboAllocators.at(uid).get();
        }

        Detail::VBOAllocator* getVBOAllocator(std::uint32_t blockSize, std::uint32_t vertexSize)
        {
            std::uint64_t uid = getUID(blockSize, vertexSize);

            if (m_vboAllocators.count(uid) == 0)
            {
                m_vboAllocators.insert(std::make_pair(uid, std::make_unique<Detail::VBOAllocator>(blockSize, vertexSize)));
            }
            return m_vboAllocators.at(uid).get();
        }

    private:
        //for some reason I can't use a unique_ptr??
        std::unordered_map<std::uint64_t, std::shared_ptr<Detail::IBOAllocator>> m_iboAllocators;
        std::unordered_map<std::uint64_t, std::shared_ptr<Detail::VBOAllocator>> m_vboAllocators;

        std::uint64_t getUID(std::uint32_t blockSize, std::uint32_t dataSize) const
        {
            std::uint64_t uid = blockSize;
            uid <<= 32;
            uid |= dataSize;
            return uid;
        }
    };
}
