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

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/GlobalConsts.hpp>

static constexpr glm::vec2 PuntBarSize = { 250.f, 20.f };
static constexpr glm::vec2 PuntBarOffset = { 20.f, 20.f };
static constexpr glm::vec2 LivesOffset = { 20.f, 60.f };
static constexpr glm::vec2 LifeSize = glm::vec2(24.f);

static constexpr glm::vec2 DiedMessageOffset = { 0.f, 120.f };

static constexpr float UIDepth = -4.f;
static constexpr float SummaryDepth = -1.f;

static inline glm::vec2 getUICorner(std::size_t playerID, std::size_t playerCount)
{
    if (playerCount == 1)
    {
        return glm::vec2(0.f);
    }

    return glm::vec2(((cro::DefaultSceneSize.x / 2) * (playerID % 2)),
        (((1 - (playerID / 2)) * (cro::DefaultSceneSize.y / 2)) * (playerCount / 3)));
}