/*-----------------------------------------------------------------------

Matt Marchant 2023
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

enum MenuIndex
{
    Player = 200,

    PlayerLeaveMenu,
    PlayerAdd,
    PlayerRemove,
    PlayerNextMenu,

    Player01,
    Player02,
    Player03,
    Player04,
    Player05,
    Player06,
    Player07,
    Player08,
    
    PlayerCPU,
    PlayerPrevProf,
    PlayerProfile,
    PlayerNextProf,

    PlayerReset,
    PlayerCreate,
    PlayerEdit,
    PlayerDelete,


    Lobby = 400,
    LobbyQuit,
    LobbyStart,
    LobbyCourseA,
    LobbyCourseB,
    LobbyRulesA,
    LobbyRulesB,
    LobbyInfoA,
    LobbyInfoB,
    PlayerManagement,

    CoursePrev,
    CourseNext,
    CourseHolePrev,
    CourseHoleNext,
    CourseInviteFriend,
    CourseReverse,
    CourseUser,
    CourseCPUSkip,
    CourseClubLimit,
    CourseNightTime,
    CourseWeather,
    CourseFriendsOnly,

    RulesPrevious,
    RulesNext,
    RulesGimmePrev,
    RulesGimmeNext,
    RulesClubset,

    InfoLeaderboards,
    InfoLeague,
    InfoScorecard,
};
