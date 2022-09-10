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

#include <crogine/detail/glm/vec3.hpp>

#include <vector>

class Path final
{
public:
    explicit Path(/*bool looped = false*/);

    void addPoint(glm::vec3 point);

    glm::vec3 getPoint(std::size_t index) const;

    float getSpeedMultiplier(std::size_t segmentIndex) const;

    float getLength() const { return m_length; }

    const std::vector<glm::vec3>& getPoints() const { return m_points; }
    std::vector<glm::vec3>& getPoints() { return m_points; }

    void reverse();

private:
    std::vector<glm::vec3> m_points;
    std::vector<float> m_speedMultipliers;
    float m_length;
    //bool m_looped;
};
