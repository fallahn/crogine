/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "Clubs.hpp"

#include <crogine/ecs/Entity.hpp>

#include <vector>
#include <array>

namespace cro
{
    struct ResourceCollection;
    class Scene;
}

struct ClubModels final
{
    std::string name;
    std::uint32_t uid = 0;

    //holds the entities for each model available
    std::vector<cro::Entity> models;

    //holds the indices for each club type into the models vector
    std::array<std::int32_t, ClubID::Count> indices = {};

    bool loadFromFile(const std::string& path, cro::ResourceCollection&, cro::Scene&);
};