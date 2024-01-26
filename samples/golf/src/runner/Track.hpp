/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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
#include "../Colordome-32.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <vector>
#include <mutex>

static inline constexpr float SegmentLength = 0.25f;
static inline constexpr float RoadWidth = 2.5f;

static inline constexpr std::size_t EnterMin = 60;
static inline constexpr std::size_t EnterMax = 90;
static inline constexpr std::size_t HoldMin = 150;
static inline constexpr std::size_t HoldMax = 400;
static inline constexpr std::size_t ExitMin = EnterMin;
static inline constexpr std::size_t ExitMax = EnterMax;

static inline constexpr float CurveMin = -0.004f;
static inline constexpr float CurveMax = 0.004f;
static inline constexpr float HillMin = -30.f;
static inline constexpr float HillMax = 30.f;

static inline constexpr std::int32_t DrawDistance = 500; //number of segs

class Track final
{
public:
    struct GenerationContext final
    {
        float hill = 0.f;
        float curve = 0.f;
        std::int32_t traffic = 0;
        std::int32_t debris = 0;
    };

    std::size_t getSegmentCount() const { return m_segments.size(); }

    void addSegment(std::size_t enter, std::size_t hold, std::size_t exit, float curve, float hill, float scale)
    {
        std::scoped_lock lock(m_mutex);

        const float z = m_pendingSegments.size() * SegmentLength;
        const std::size_t start = m_pendingSegments.size();

        std::size_t i = 0;
        std::size_t end = enter;

        const auto setColour =
            [&](TrackSegment& seg)
            {
                seg.roadColour = CD32::Colours[CD32::GreyDark];

                if (((start + i) / 9) % 2)
                {
                    seg.rumbleColour = CD32::Colours[CD32::Red];
                    
                    auto c = CD32::Colours[CD32::GreenMid].getVec4();
                    c *= 0.93f;
                    c.a = 1.f;
                    seg.grassColour = c;
                }
                else
                {
                    seg.rumbleColour = CD32::Colours[CD32::BeigeLight];
                    seg.grassColour = CD32::Colours[CD32::GreenMid];
                }

                seg.roadMarking = (((start + i) / 6) % 2);

                seg.uv.x = 0.f;
                seg.uv.y = seg.position.z / (1.f / SegmentLength);

                seg.index = m_pendingSegments.size() - 1;
            };

        for (; i < end; ++i)
        {
            const float progress = static_cast<float>(i) / enter;
            glm::vec3 pos(0.f, hill * cro::Util::Easing::easeOutSine(progress), z + (i * SegmentLength));
            scale = glm::mix(1.f, scale, progress);
            setColour(m_pendingSegments.emplace_back(pos, SegmentLength, RoadWidth * scale, curve * cro::Util::Easing::easeInSine(progress)));
        }

        end += hold;
        for (; i < end; ++i)
        {
            glm::vec3 pos(0.f, hill, z + (i * SegmentLength));
            setColour(m_pendingSegments.emplace_back(pos, SegmentLength, RoadWidth * scale, curve));
        }

        end += exit;
        for (; i < end; ++i)
        {
            const float progress = static_cast<float>(i - enter - hold) / exit;
            glm::vec3 pos(0.f, hill * (cro::Util::Easing::easeOutSine(1.f - progress)), z + (i * SegmentLength));
            scale = glm::mix(scale, 1.f, progress);
            setColour(m_pendingSegments.emplace_back(pos, SegmentLength, RoadWidth * scale, curve * (cro::Util::Easing::easeOutSine(1.f - progress))));
        }
    }

    //swaps in pending buffer of road segments
    void swap(cro::Scene& scene);

    std::vector<TrackSegment>& getPendingSegments() { return m_pendingSegments; }

    const TrackSegment& operator[](std::size_t i)const { return m_segments[i]; }
    TrackSegment& operator[](std::size_t i) { return m_segments[i]; }

private:
    std::vector<TrackSegment> m_pendingSegments;
    std::vector<TrackSegment> m_segments;

    std::mutex m_mutex;
};