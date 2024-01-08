/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/core/ProfileTimer.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/graphics/BoundingBox.hpp>

#include <array>

class ChunkVisSystem final : public cro::System
{
public:
    ChunkVisSystem(cro::MessageBus&, glm::vec2 Mapsize);

    void process(float) override;

    //we want to make the AABBs as toight as possible so
    //let's make this variable so we can update it on model change
    void setWorldHeight(float);


    //unique index created by ORing together visible indices
    std::int32_t getIndex() const { return m_currentIndex; }

    //list of indices currently visible
    std::vector<std::int32_t> getIndexList() const { return m_indexList; }

    glm::vec2 getChunkSize() const { return m_chunkSize; }

    static constexpr std::int32_t RowCount = 3;
    static constexpr std::int32_t ColCount = 4;

#ifdef CRO_DEBUG_
    //checks all boxes against frustum
    std::int32_t narrowphaseCount = 0;
    float getNarrowphaseTime() const { return m_narrowphaseTimer.result(); }
#endif
private:
    glm::vec2 m_chunkSize;
    std::int32_t m_currentIndex;
    std::vector<std::int32_t> m_indexList; //TODO would a fixed size array be faster?

    std::array<cro::Box, RowCount* ColCount> m_boundingBoxes = {};

#ifdef CRO_DEBUG_
    cro::ProfileTimer<30> m_narrowphaseTimer;
#endif
};