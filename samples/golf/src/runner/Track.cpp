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

#include "Track.hpp"
#include "CarSystem.hpp"

void Track::swap(cro::Scene& scene)
{
    for (auto& seg : m_segments)
    {
        for (auto e : seg.cars)
        {
            e.getComponent<Car>().active = false;
            scene.destroyEntity(e);
        }
    }
    {
        std::scoped_lock lock(m_mutex);
        m_segments.swap(m_pendingSegments);
    }
    for (auto& seg : m_segments)
    {
        for (auto e : seg.cars)
        {
            e.getComponent<Car>().active = true;
        }
    }
}