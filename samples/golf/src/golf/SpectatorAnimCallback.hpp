/*-----------------------------------------------------------------------

Matt Marchant 2022
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

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/Random.hpp>

#include <array>
#include <vector>

class SpectatorCallback final
{
public:
    explicit SpectatorCallback(const std::vector<cro::SkeletalAnim>& anims)
        : m_nextAnimTime(cro::seconds(cro::Util::Random::value(6.f, 16.f)))
    {
        std::fill(m_animations.begin(), m_animations.end(), 0);
        CRO_ASSERT(!anims.empty(), ""); //asserting this doesn't help much when loading external files...
        for (auto i = 0u; i < anims.size(); ++i)
        {
            const auto& name = anims[i].name;
            if (name == "idle")
            {
                m_animations[AnimID::Idle] = i;
            }
            else if (name == "clap")
            {
                m_animations[AnimID::Clap] = i;
            }
            else if (name == "random")
            {
                m_animations[AnimID::Random] = i;
            }
        }
    }

    void operator() (cro::Entity e, float dt)
    {
        if (!e.getComponent<cro::Model>().isHidden())
        {
            auto& skel = e.getComponent<cro::Skeleton>();
            auto current = skel.getCurrentAnimation();
            if (current == m_animations[AnimID::Idle])
            {
                if (m_animationClock.elapsed() > m_nextAnimTime)
                {
                    skel.play(m_animations[AnimID::Random], 1.f, 0.5f);
                    m_animationClock.restart();
                    m_nextAnimTime = cro::seconds(cro::Util::Random::value(6.f, 16.f));
                }
            }
            else
            {
                if (skel.getState() == cro::Skeleton::Stopped)
                {
                    skel.play(m_animations[AnimID::Idle], 1.f, 0.5f);
                    m_nextAnimTime = cro::seconds(cro::Util::Random::value(6.f, 16.f));
                    m_animationClock.restart();
                }
            }

            if (auto& clap = e.getComponent<cro::Callback>().getUserData<bool>(); clap)
            {
                skel.play(m_animations[AnimID::Clap], 1.f, 0.5f);
                m_animationClock.restart();
                clap = false;
            }
        }
        ////e.getComponent<cro::Callback>().setUserData<bool>(false);
    }

private:

    struct AnimID final
    {
        enum
        {
            Idle, Clap, Random,
            Count
        };
    };
    std::array<std::int32_t, AnimID::Count> m_animations = {};

    cro::Time m_nextAnimTime;
    cro::Clock m_animationClock;
};