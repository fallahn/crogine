/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Transform.hpp>


struct InterpolationPoint final
{
	InterpolationPoint() = default;
	InterpolationPoint(glm::vec3 p, glm::vec3 v, glm::quat r, std::int32_t ts)
		: position(p), velocity(v), rotation(r), timestamp(ts) {}

	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 velocity = glm::vec3(0.f);
	glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
	std::int32_t timestamp = 0;
};

enum class InterpolationType
{
	Linear, Hermite
};

template <InterpolationType interpolationType>
class InterpolationComponent final
{
public:
	explicit InterpolationComponent(InterpolationPoint ip = {})
	{
		m_buffer.push_back(ip);
		m_wantsBuffer = m_buffer.size() < m_bufferSize;
	}
	
	std::int32_t getElapsedTime() const
	{
		return m_overflow + m_timer.elapsed().asMilliseconds();
	}

	void addPoint(InterpolationPoint ip)
	{
		CRO_ASSERT(m_bufferSize > 1, "");
		if (ip.timestamp > m_buffer.back().timestamp)
		{
			//makes sure timer doesn't start until finished buffering
			if (m_wantsBuffer)
			{
				m_timer.restart();
				m_overflow = 0;
			}

			m_buffer.push_back(ip);

			//if theres a large time gap, shrink the front
			//so we can catch up a bit
			if (m_buffer[1].timestamp - m_buffer[0].timestamp > MaxTimeGap)
			{
				m_buffer.front().timestamp = m_buffer[1].timestamp - 10;
			}

			//this shouldn't happen, but as precaution...
			if (m_buffer.size() == m_buffer.capacity())
			{
				m_buffer.pop_front();
			}

			m_wantsBuffer = m_buffer.size() < m_bufferSize;
		}
	}

	//returns interpolated velocity if using Hermite type
	glm::vec3 getVelocity() const
	{
		return m_interpVelocity;
	}

	std::uint32_t id = std::numeric_limits<std::uint32_t>::max();

private:
	cro::Clock m_timer;
	std::int32_t m_overflow = 0;

	CircularBuffer<InterpolationPoint, 18u> m_buffer;

	std::size_t m_bufferSize = 3;
	bool m_wantsBuffer = true;

	glm::vec3 m_interpVelocity = glm::vec3(0.f);

	static constexpr std::int32_t MaxTimeGap = 250;

	template <InterpolationType>
	friend class InterpolationSystem;
};

template <InterpolationType Interpolation = InterpolationType::Linear>
class InterpolationSystem final : public cro::System
{
public:
	explicit InterpolationSystem(cro::MessageBus& mb)
		: cro::System(mb, typeid(InterpolationSystem<Interpolation>))
	{
		requireComponent<cro::Transform>();
		requireComponent<InterpolationComponent<Interpolation>>();
	}

	void process(float) override
	{
		for (auto entity : getEntities())
		{
			auto& interp = entity.getComponent<InterpolationComponent<Interpolation>>();

			if (interp.m_buffer.size() > 1)
			{
				//if the buffer is full enough...
				if (!interp.m_wantsBuffer)
				{
					std::int32_t difference = interp.m_buffer[1].timestamp - interp.m_buffer[0].timestamp;
					CRO_ASSERT(difference > 0, "");

					std::int32_t elapsed = interp.getElapsedTime();
					while (elapsed > difference)
					{
						interp.m_overflow = elapsed - difference;
						interp.m_timer.restart();
						interp.m_buffer.pop_front();

						entity.getComponent<cro::Transform>().setPosition(interp.m_buffer[0].position);
						entity.getComponent<cro::Transform>().setRotation(interp.m_buffer[0].rotation);

						elapsed = interp.m_overflow;

						if (interp.m_buffer.size() == 1)
						{
							//don't do anything until we buffered more input
							interp.m_wantsBuffer = true;
							break;
						}
						else
						{
							difference = interp.m_buffer[1].timestamp - interp.m_buffer[0].timestamp;
						}
					}

					if (!interp.m_wantsBuffer)
					{
						float t = static_cast<float>(elapsed) / difference;

						//apply interpolated transform to entity

						if constexpr (Interpolation == InterpolationType::Hermite)
						{
							//hermite - https://stackoverflow.com/questions/55302066/implement-hermite-interpolation-multiplayer-game

							float t2 = t * t;
							float t3 = t2 * t;

							auto startPos = interp.m_buffer[0].position;
							auto startVel = interp.m_buffer[0].velocity;
							auto endPos = interp.m_buffer[1].position;
							auto endVel = interp.m_buffer[1].velocity;
							float duration = static_cast<float>(difference) / 1000.f;

							glm::vec3 position =
								(2.f * t3 - 3.f * t2 + 1.f) * startPos +
								(t3 - 2.f * t2 + t)         * duration * startVel +
								(-2.f * t3 + 3.f * t2)      * endPos +
								(t3 - t2)                   * duration * endVel;

							entity.getComponent<cro::Transform>().setPosition(position);

							interp.m_interpVelocity = 1.f / duration * (
								(6.f * t2 - 6.f * t)       * startPos +
								(3.f * t2 - 4.f * t + 1.f) * duration * startVel +
								(-6.f * t2 + 6.f * t)      * endPos +
								(3.f * t2 - 2.f * t)       * duration * endVel);
						}
						else
						{
							//linear
							auto diff = interp.m_buffer[1].position - interp.m_buffer[0].position;
							auto position = interp.m_buffer[0].position + (diff * t);

							entity.getComponent<cro::Transform>().setPosition(position);
						}

						auto rotation = glm::slerp(interp.m_buffer[0].rotation, interp.m_buffer[1].rotation, t);
						entity.getComponent<cro::Transform>().setRotation(rotation);
					}
				}
			}
			else
			{
				interp.m_wantsBuffer = true;
			}
		}
	}

private:

};