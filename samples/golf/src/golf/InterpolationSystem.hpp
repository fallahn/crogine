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

template <InterpolationType>
struct InterpolationComponent final
{
	explicit InterpolationComponent(InterpolationPoint ip = {})
	{
		buffer.push_back(ip);
		wantsBuffer = buffer.size() < bufferSize;
	}
	
	cro::Clock timer;
	std::int32_t overflow = 0;
	std::int32_t getElapsedTime() const
	{
		return overflow + timer.elapsed().asMilliseconds();
	}


	CircularBuffer<InterpolationPoint, 18u> buffer;
	void addPoint(InterpolationPoint ip)
	{
		CRO_ASSERT(bufferSize > 1, "");
		if (ip.timestamp > buffer.back().timestamp)
		{
			//makes sure timer doesn't start until finished buffering
			if (wantsBuffer)
			{
				timer.restart();
				overflow = 0;
			}

			buffer.push_back(ip);

			//if theres a large time gap, pop the front
			//so we can catch up a bit
			if (buffer[1].timestamp - buffer[0].timestamp > MaxTimeGap
				|| buffer.size() == buffer.capacity())
			{
				buffer.pop_front();
			}

			wantsBuffer = buffer.size() < bufferSize;
		}
	}
	
	std::size_t bufferSize = 3;
	bool wantsBuffer = true;

	std::uint32_t id = std::numeric_limits<std::uint32_t>::max();

	static constexpr std::int32_t MaxTimeGap = 250;
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

			if (interp.buffer.size() > 1)
			{
				//if the buffer is full enough...
				if (!interp.wantsBuffer)
				{
					std::int32_t difference = interp.buffer[1].timestamp - interp.buffer[0].timestamp;
					CRO_ASSERT(difference > 0, "");

					std::int32_t elapsed = interp.getElapsedTime();
					while (elapsed > difference)
					{
						interp.overflow = elapsed - difference;
						interp.timer.restart();
						interp.buffer.pop_front();

						entity.getComponent<cro::Transform>().setPosition(interp.buffer[0].position);
						entity.getComponent<cro::Transform>().setRotation(interp.buffer[0].rotation);

						elapsed = interp.overflow;

						if (interp.buffer.size() == 1)
						{
							//don't do anything until we buffered more input
							interp.wantsBuffer = true;
							break;
						}
						else
						{
							difference = interp.buffer[1].timestamp - interp.buffer[0].timestamp;
						}
					}

					if (!interp.wantsBuffer)
					{
						float t = static_cast<float>(elapsed) / difference;

						//apply interpolated transform to entity

						if constexpr (Interpolation == InterpolationType::Hermite)
						{
							//hermite - https://stackoverflow.com/questions/55302066/implement-hermite-interpolation-multiplayer-game

							float t2 = t * t;
							float t3 = t2 * t;

							auto startPos = interp.buffer[0].position;
							auto startVel = interp.buffer[0].velocity;
							auto endPos = interp.buffer[1].position;
							auto endVel = interp.buffer[1].velocity;
							float duration = static_cast<float>(difference) / 1000.f;

							glm::vec3 position =
								(2.f * t3 - 3.f * t2 + 1.f) * startPos +
								(t3 - 2.f * t2 + t)         * duration * startVel +
								(-2.f * t3 + 3.f * t2)      * endPos +
								(t3 - t2)                   * duration * endVel;

							entity.getComponent<cro::Transform>().setPosition(position);
						}
						else
						{
							//linear
							auto diff = interp.buffer[1].position - interp.buffer[0].position;
							auto position = interp.buffer[0].position + (diff * t);

							entity.getComponent<cro::Transform>().setPosition(position);
						}

						auto rotation = glm::slerp(interp.buffer[0].rotation, interp.buffer[1].rotation, t);
						entity.getComponent<cro::Transform>().setRotation(rotation);
					}
				}
			}
			else
			{
				interp.wantsBuffer = true;
			}
		}
	}

private:

};