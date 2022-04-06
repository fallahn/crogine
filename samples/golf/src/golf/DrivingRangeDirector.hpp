/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "HoleData.hpp"

#include <crogine/ecs/Director.hpp>

#include <vector>
 
static constexpr glm::vec3 PlayerPosition(0.f, 2.5f, 121.f);

class DrivingRangeDirector final : public cro::Director
{
public:
    explicit DrivingRangeDirector(std::vector<HoleData>&);

    void handleMessage(const cro::Message&) override;

    void process(float) override;

    void setHoleCount(std::int32_t count, std::int32_t holeIndex = -1);

    //note that this does not necessarily
    //end when it reaches zero as the hole count
    //may be greater than the number of holes.
    std::int32_t getCurrentHole() const;

    std::int32_t getCurrentStroke() const { return m_totalHoleCount - m_holeCount; }

    std::int32_t getTotalStrokes() const { return m_totalHoleCount; }

    float getScore(std::int32_t idx) const { return m_scores[idx]; };

    bool roundEnded() const { return m_holeCount == 0; }

    static constexpr std::int32_t MaxHoles = 18;


private:

    std::vector<HoleData>& m_holeData;
    std::int32_t m_holeCount;
    std::int32_t m_totalHoleCount;
    std::int32_t m_holeIndex;

    std::array<float, MaxHoles> m_scores = {};
};
