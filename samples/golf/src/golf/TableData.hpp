/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include <crogine/core/String.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <vector>
#include <array>

struct CollisionID final
{
    enum
    {
        Table, Cushion,
        Ball, Pocket,

        Count
    };

    static const std::array<std::string, Count> Labels;
};

struct SnookerID final
{
    enum
    {
        White, Red,
        Yellow, Green, Brown,
        Blue, Pink, Black,

        Count
    };
};

static constexpr std::int8_t CueBall = 0;

struct PocketInfo final
{
    glm::vec2 position = glm::vec2(0.f);
    std::int32_t value = 0;
    float radius = 0.06f;
};

struct TableData //client data inherits this
{
    std::string name;
    std::string collisionModel;
    std::string viewModel;
    std::vector<PocketInfo> pockets;
    cro::FloatRect spawnArea;

    enum Rules
    {
        Eightball,
        Nineball,
        BarBillliards,
        Snooker,

        Void,
        Count
    }rules = Void;
    static const std::array<std::string, Rules::Count> RuleStrings;

    std::vector<std::string> tableSkins;
    std::vector<std::string> ballSkins;

    bool loadFromFile(const std::string&);
};

static const std::array<cro::String, TableData::Rules::Count> TableStrings =
{
    cro::String("Eight Ball"),
    cro::String("Nine Ball"),
    cro::String("Bar Billiards"),
    cro::String("Snooker"),
    cro::String("Void")
};