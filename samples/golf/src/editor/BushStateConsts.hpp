/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2026
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

#include <crogine/detail/glm/vec2.hpp>

//we'll assume 64px per metre
static constexpr float PixelsPerMetre = 64.f;
static constexpr glm::uvec2 BillboardTargetSize(640, 448);

static constexpr float MinModelScale = 0.1f;

static constexpr std::uint64_t RenderFlagsBillboard = 1;
static constexpr std::uint64_t RenderFlagsThumbnail = 2;

//MapSize * 0.375
static constexpr glm::uvec2 ThumbnailSize(120u, 75u);
//static constexpr glm::uvec2 ThumbnailSize(202u, 120u);