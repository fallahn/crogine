/*-----------------------------------------------------------------------

Matt Marchant 2024
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


Based on articles: http://www.extentofthejam.com/pseudo/
                   https://codeincomplete.com/articles/javascript-racer/

-----------------------------------------------------------------------*/

#pragma once

#include "TrackSegment.hpp"

#include <crogine/util/Easings.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <vector>

static inline constexpr float SegmentLength = 250.f;
static inline constexpr float RoadWidth = 2500.f;

static inline constexpr std::size_t EnterMin = 30;
static inline constexpr std::size_t EnterMax = 100;
static inline constexpr std::size_t HoldMin = 150;
static inline constexpr std::size_t HoldMax = 500;
static inline constexpr std::size_t ExitMin = EnterMin;
static inline constexpr std::size_t ExitMax = EnterMax;

static inline constexpr float CurveMin = -10.f;
static inline constexpr float CurveMax = 10.f;
static inline constexpr float HillMin = -60.f;
static inline constexpr float HillMax = 60.f;

static inline constexpr std::int32_t DrawDistance = 100; //number of segs

class Track final
{
public:
    std::size_t getSegmentCount() const { return m_segments.size(); }

    void addSegment(std::size_t enter, std::size_t hold, std::size_t exit, float curve, float hill)
    {
        const float z = m_segments.size() * SegmentLength;
        std::size_t i = 0;
        std::size_t end = enter;

        for (; i < end; ++i)
        {
            const float progress = static_cast<float>(i) / enter;
            glm::vec3 pos(0.f, hill * cro::Util::Easing::easeInOutSine(progress), z + (i * SegmentLength));
            m_segments.emplace_back(pos, SegmentLength, RoadWidth, curve * cro::Util::Easing::easeInSine(progress));
        }

        end += hold;
        for (; i < end; ++i)
        {
            glm::vec3 pos(0.f, hill, z + (i * SegmentLength));
            m_segments.emplace_back(pos, SegmentLength, RoadWidth, curve);
        }

        end += exit;
        for (; i < end; ++i)
        {
            const float progress = static_cast<float>(i - enter - hold) / exit;
            glm::vec3 pos(0.f, 1.f - (hill * cro::Util::Easing::easeInOutSine(progress)), z + (i * SegmentLength));
            m_segments.emplace_back(pos, SegmentLength, RoadWidth, 1.f - (curve * cro::Util::Easing::easeOutSine(progress)));
        }
    }

    const TrackSegment& operator[](std::size_t i)const { return m_segments[i]; }

private:
    std::vector<TrackSegment> m_segments;
};