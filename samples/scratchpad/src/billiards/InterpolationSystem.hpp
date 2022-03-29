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

struct InterpolationPoint final
{
	InterpolationPoint() = default;
	InterpolationPoint(glm::vec3 p, glm::quat r, std::int32_t ts)
		: position(p), rotation(r), timestamp(ts) {}

	glm::vec3 position = glm::vec3(0.f);
	glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
	std::int32_t timestamp = 0;
};

struct InterpolationComponent final
{
	explicit InterpolationComponent(InterpolationPoint ip = {})
	{
		buffer.push_back(ip);
	}
	
	cro::Clock timer;
	std::int32_t overflow = 0;

	std::int32_t getElapsedTime() const
	{
		return overflow + timer.elapsed().asMilliseconds();
	}

	CircularBuffer<InterpolationPoint, 8u> buffer;
	
	void addPoint(InterpolationPoint ip)
	{
		if (buffer.back().timestamp < ip.timestamp)
		{
			buffer.push_back(ip);
		}
	}

	std::uint32_t id = std::numeric_limits<std::uint32_t>::max();

	std::size_t bufferSize = 3;
	bool wantsBuffer = true;
};

class InterpolationSystem final : public cro::System
{
public:
	explicit InterpolationSystem(cro::MessageBus&);

	void process(float) override;

private:

};