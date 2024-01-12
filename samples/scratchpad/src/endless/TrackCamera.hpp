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

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <crogine/util/Constants.hpp>

#include <cmath>

class TrackCamera final
{
public:
    TrackCamera()
    {
        setFOV(m_fov);
    }

    glm::vec3 getPosition() const { return m_position; }
    float getFOV() const { return m_fov; }
    float getDepth() const { return m_depth; }

    void updateScreenProjection(TrackSegment& segment, glm::vec3 playerPos, glm::vec2 screenSize)
    {
        auto translation = segment.position - m_position;
        translation.x -= playerPos.x;
        //translation.y -= playerPos.y;

        const float scale = (m_depth / translation.z);
        auto projection = translation * scale;
        projection.z = segment.width * scale;

        segment.projection.position.x = (screenSize.x / 2.f) * (1.f + projection.x);
        segment.projection.position.y = (screenSize.y / 2.f) * (1.f + projection.y);
        segment.projection.width = (screenSize.x / 2.f) * projection.z;
        segment.projection.scale = scale;
    }

    void setPosition(glm::vec3 p) { m_position = p; }
    void setFOV(float fov)
    {
        m_fov = fov;
        m_depth = 1.f / std::tan((m_fov / 2.f) * cro::Util::Const::degToRad); 
    }

    void move(glm::vec3 m) { m_position += m; }
    void setX(float f) { m_position.x = f; }
    void setY(float f) { m_position.y = f; }
    void setZ(float f) { m_position.z = f; }

private:
    float m_fov = 75.f;
    float m_depth = 1.f; //normalised value based on FOV of camera

    glm::vec3 m_position = glm::vec3(0.f, 1.8f, 0.3f);
};