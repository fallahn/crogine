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

#include "InterpolationSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>

namespace
{
	//we're expecting ~ 50ms intervals so usually 250
	//could be considered a 'large' gap.

	//constexpr std::int32_t MaxTimeGap = 250;
	constexpr std::int32_t MaxTimeGap = 5000;
}

InterpolationComponent::InterpolationComponent(InterpolationPoint ip)
{
	buffer.push_back(ip);
	wantsBuffer = buffer.size() < bufferSize;
}

std::int32_t InterpolationComponent::getElapsedTime() const
{
	return overflow + timer.elapsed().asMilliseconds();
}

void InterpolationComponent::addPoint(InterpolationPoint ip)
{
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
		if (buffer[1].timestamp - buffer[0].timestamp > MaxTimeGap)
		{
			buffer.pop_front();
		}

		wantsBuffer = buffer.size() < bufferSize;
	}
}

//--------------------------------------//

InterpolationSystem::InterpolationSystem(cro::MessageBus& mb)
	: cro::System(mb, typeid(InterpolationSystem))
{
	requireComponent<cro::Transform>();
	requireComponent<InterpolationComponent>();
}

//public
void InterpolationSystem::process(float)
{
	for (auto entity : getEntities())
	{
		auto& interp = entity.getComponent<InterpolationComponent>();
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

					elapsed = 0;

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
					float interpTime = static_cast<float>(elapsed) / difference;

					//apply interpolated transform to entity
					auto diff = interp.buffer[1].position - interp.buffer[0].position;
					auto position = interp.buffer[0].position + (diff * interpTime);
					auto rotation = glm::slerp(interp.buffer[0].rotation, interp.buffer[1].rotation, interpTime);

					entity.getComponent<cro::Transform>().setPosition(position);
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