/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#include "InterpolationSystem.hpp"

#include <crogine/detail/Assert.hpp>

namespace
{

}

InterpolationComponent::InterpolationComponent(InterpolationPoint initialPoint)
    : m_enabled         (true),
    m_targetPoint       (initialPoint),
    m_previousPoint     (initialPoint),
    m_timeDifference    (1),
    m_currentTime       (0),
    m_id                (0)
{
    m_elapsedTimer.restart();
    m_targetPoint.timestamp++;
}

//public
void InterpolationComponent::addTarget(const InterpolationPoint& target)
{
    if(m_buffer.size() < m_buffer.capacity())
    {
        if (m_buffer.empty() || m_buffer.back().timestamp < target.timestamp)
        {
            m_buffer.push_back(target);

            //if this is the first target in a while
            //check for a large delta and try to mitigate it
            if (m_buffer.size() == 1)
            {
                auto delta = target.timestamp - m_targetPoint.timestamp;
                if (delta > 167) //10 frames ish
                {
                    m_previousPoint = m_targetPoint = target;

                    m_targetPoint.timestamp = target.timestamp - 1;
                    m_previousPoint.timestamp = m_targetPoint.timestamp - 1;

                    m_elapsedTimer.restart();
                }
            }
        }
    }

    //if (!m_enabled)
    //{
    //    //this *might* help keep things smoother (hard to tell) but it def causes lag between the cue/club anim
    //    //we can tune this for a local client, however it is also variable in time depending on the client's lag...
    //    m_enabled = m_buffer.size() > 6; 
    //}
}

void InterpolationComponent::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool InterpolationComponent::getEnabled() const
{
    return m_enabled;
}

void InterpolationComponent::resetPosition(glm::vec3 position)
{
    m_previousPoint.position = m_targetPoint.position = position;
}

void InterpolationComponent::resetRotation(glm::quat rotation)
{
    m_previousPoint.rotation = m_targetPoint.rotation = rotation;
}

//private
void InterpolationComponent::applyNextTarget()
{
    if (!m_buffer.empty())
    {
        CRO_ASSERT(!std::isnan(m_previousPoint.position.x), "");

        auto target = m_buffer.pop_front();
        
        //time difference must never be 0!!
        if (target.timestamp > m_previousPoint.timestamp)
        {
            m_previousPoint = m_targetPoint;
            m_currentTime %= m_timeDifference;

            m_timeDifference = target.timestamp - m_previousPoint.timestamp;
            m_targetPoint = target;
        }
    }
}