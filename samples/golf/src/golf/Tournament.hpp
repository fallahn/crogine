/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "League.hpp"

#include <array>
#include <string>

static inline const std::array<std::array<std::string, 4u>, 2u> TournamentCourses =
{
    std::array<std::string, 4u>
    {
        std::string("course_10"), "course_07", "course_08", "course_01"
    },
    std::array<std::string, 4u>
    {
        std::string("course_12"), "course_05", "course_11", "course_02"
    }
};

//note that this is read/written directly as a binary blob
//so any changes in the future must be via the reserved bytes
struct Tournament final
{
    std::int32_t id = 0; //0 or 1 used to choose the course data array
    std::int32_t round = 0; //current active round used to index into the array chosen by the above
    LeaguePlayer opponentStats; //current opponent used to set FriendlyPlayer in GolfState
    std::array<std::int32_t, 9u> opponentScores = {}; //opponent's progress in current round
    std::array<std::int32_t, 9u> scores = {}; //our current scores. If first is zero we assume round is not yet started
    std::array<std::int32_t, 16u> tier0 = {}; //IDs of players in the first tier, 8L and 8R
    std::array<std::int32_t, 8u> tier1 = {}; //player IDs for round 2, -2 indicates no player assigned (-1 is human player index)
    std::array<std::int32_t, 4u> tier2 = {};
    std::array<std::int32_t, 2u> tier3 = {};

    std::array<std::int32_t, 24u> Padding = {}; //reserve space in case we expand this in the future

    Tournament();
};

struct TournamentIndex final
{
    enum
    {
        NullVal = -1,
        A, B
    };
};

//clears the brackets and shuffles tier0
void resetTournament(Tournament&);

//write progress file
void writeTournamentData(const Tournament&);

//read progress file
void readTournamentData(Tournament&);