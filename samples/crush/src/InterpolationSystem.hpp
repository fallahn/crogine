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

#include "CircularBuffer.hpp"
#include <crogine/core/Clock.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

/*!
\brief Contains information required for a inperpolation to occur
*/
struct InterpolationPoint final
{
    InterpolationPoint(glm::vec3 pos = glm::vec3(0.f), glm::quat rot = glm::quat(1.f,0.f,0.f,0.f), std::int32_t ts = 0, glm::vec2 v = glm::vec2(0.f))
        : position(pos), rotation(rot), timestamp(ts), velocity(v){}

    glm::vec3 position = glm::vec3(0.f);
    glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    std::int32_t timestamp = 0;

    glm::vec2 velocity = glm::vec2(0.f);
};

/*!
\brief Interpolates position and rotation received from a server.
When receiving infrequent (say 100ms or so) position updates from
a remote server entities can have their position interpolated via
this component. The component, when coupled with an InterpolationSystem
will travel towards the given target position using the given timestamp
to linearly interpolate movement. This component is not limited to
networked entities, and can be used anywhere linear interpolation of
movement is desired, for example path finding.

Requires:
    CircularBuffer.hpp
    InterpolationSystem.hpp
    InterpolationSystem.cpp
    InterpolationComponent.cpp
*/
class InterpolationComponent final
{
public:
    /*!
    \brief Constructor
    Interpolation components should be passed an interpolation
    point containing the initial transform and server time of the
    actor to prevent large lags between the default timestamp (0)
    and the first server update
    */
    explicit InterpolationComponent(InterpolationPoint = {});

    /*!
    \brief Sets the target position and timestamp.
    The timestamp is used in conjunction with the previous timestamp
    to decide how quickly motion should be integrated between positions.
    The timestamp would usually be in server time, and arrive in the packet
    data with the destination postion, in milliseconds.
    \param pos Target destination
    \param timestamp Time at which this position should be reached, relative
    to the previous position.
    */
    void setTarget(const InterpolationPoint&);

    /*!
    \brief Sets whether or not this component is enabled
    */
    void setEnabled(bool);

    /*!
    \brief Returns whether or not this component is enabled
    */
    bool getEnabled() const;

    /*!
    \brief Overrides the current position with the given position
    */
    void resetPosition(glm::vec3);

    /*!
    \brief Overrides the rotation with the given rotation
    */
    void resetRotation(glm::quat);

    void resetVelocity() { m_targetPoint.velocity = glm::vec2(0.f); }

private:
    bool m_enabled;
    InterpolationPoint m_targetPoint;
    InterpolationPoint m_previousPoint;

    cro::Clock m_elapsedTimer;
    std::int32_t m_timeDifference;

    CircularBuffer<InterpolationPoint, 4u> m_buffer;
    bool m_started;

    friend class InterpolationSystem;

    void applyNextTarget();
};

/*!
\brief Uses the InterpolationComponent to linearly
interpolate a transform component between two points.
\see InterpolationComponent
*/
class InterpolationSystem : public cro::System
{
public:
    explicit InterpolationSystem(cro::MessageBus&);

    void process(float) override;

private:

};
