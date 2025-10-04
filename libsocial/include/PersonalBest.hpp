/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include <cstdint>

#include <array>
#include <vector>
#include <algorithm>

class PersonalBest final
{
public:
    PersonalBest();

    void load();
    void save() const;

    void insertScore(std::size_t index, std::int32_t score, const std::vector<std::uint8_t>& holeScores);
    
    void fetchScore(std::size_t index, std::int32_t& scoreDst, std::vector<std::uint8_t>& holeDst) const;

    std::int32_t getRoundScore(std::size_t index) const;
private:

    struct ScoreEntry final
    {
        std::int32_t score = 0;
        std::array<std::uint8_t, 18u> holeScores = {};

        ScoreEntry()
        {
            std::fill(holeScores.begin(), holeScores.end(), 0);
        }
    };

    //hmmm shouldn't we have this const somewhere?
    std::array<ScoreEntry, 13 * 3> m_entries = {};
};