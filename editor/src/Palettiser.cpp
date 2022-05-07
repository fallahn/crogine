/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "Palettiser.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
	constexpr std::int32_t GridSize = 16;
}


float diff(glm::ivec3 a, glm::ivec3 b)
{
	return glm::length(glm::vec3(a - (b * GridSize)));
}

glm::ivec2 mapping(std::int32_t r, std::int32_t g, std::int32_t b)
{
	std::int32_t idx = r + (g * GridSize) + (b * GridSize * GridSize);
	return { idx / 64, idx % 64 };
}

bool pt::processPalette(const cro::Image& image, const std::string& outPath)
{
	if (image.getSize().x == 0 || image.getSize().y == 0)
	{
		LogE << "Image was empty" << std::endl;
		return false;
	}

	if (image.getFormat() != cro::ImageFormat::RGB
		&& image.getFormat() != cro::ImageFormat::RGBA)
	{
		LogI << "Image not RGB or RGBA" << std::endl;
		return false;
	}

	cro::Image outImage;
	outImage.create(64, 64, cro::Colour::White);

	std::int32_t stride = image.getFormat() == cro::ImageFormat::RGB ? 3 : 4;
	auto fetchPixel = [&, stride](std::int32_t idx)->glm::ivec3
	{
		auto i = idx * stride;
		const auto* px = image.getPixelData();
		return { px[i], px[i + 1], px[i + 2] };
	};

	const auto pixelCount = image.getSize().x * image.getSize().y;
	for (auto b = 0; b < GridSize; ++b)
	{
		for (auto g = 0; g < GridSize; ++g) 
		{
			for (auto r = 0; r < GridSize; ++r)
			{
				std::int32_t closestIdx = 0;
				float closest = diff(fetchPixel(0), glm::ivec3(r,g,b));

				for (auto i = 1u; i < pixelCount; ++i)
				{
					auto d = diff(fetchPixel(i), glm::ivec3(r, g, b));
					if (d < closest)
					{
						closest = d;
						closestIdx = i;
					}
				}

				auto position = mapping(r, g, b);
				auto outColour = fetchPixel(closestIdx);
				outImage.setPixel(position.x, position.y, cro::Colour(std::uint8_t(outColour.r), outColour.g, outColour.b));
			}
		}
	}

	return outImage.write(outPath);
}