/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include <crogine/ecs/Director.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/core/ConsoleClient.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

struct TargetTransform final
{
    glm::vec3 endPosition = glm::vec3(0.f);
    glm::quat endRotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    
    glm::vec3 startPosition = glm::vec3(0.f);
    glm::quat startRotation = glm::quat(1.f, 0.f, 0.f, 0.f);

    bool active = false;
    float interpolation = 0.f;
};

struct ChildNode final
{
    cro::Entity sunNode;
    cro::Entity moonNode;
};

class DayNightDirector final : public cro::Director, public cro::ConsoleClient
{
public:
    DayNightDirector();

    void handleMessage(const cro::Message&) override;

    void process(float) override;

    void setTimeOfDay(std::uint32_t minutes);

    void setTimeOfDay(float normalised);

    void setCycleSpeed(float);

private:

    float m_timeOfDay; //normalised.
    float m_targetTime; //so we can interpolate time changes from the server without jumping too much
    bool m_correctTime; //time target was changed so we're expecting correction
    float m_cycleSpeed;

    std::size_t m_currentSkyIndex;
    std::size_t m_nextSkyIndex;
};
