/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/graphics/Image.hpp>
#include <crogine/core/String.hpp>

//just to detect client/server version mismatch
//(terrain data changed between 100 -> 110)
//(model format changed between 120 -> 130)
//(server layout updated 140 -> 150)
//(skybox format changed 150 -> 160)
//(hole count became game rule 170 -> 180)
//(connection method changed 190 -> 1100)
static constexpr std::uint16_t CURRENT_VER = 1100;
static const std::string StringVer("1.10.0");


class Social final
{
public:
    struct InfoID final
    {
        enum
        {
            Menu,
            Lobby,
            Course,
            Billiards
        };
    };

    struct MessageID final
    {
        enum
        {
            SocialMessage = 10000
        };
    };

    struct SocialEvent final
    {
        enum
        {
            LevelUp,
            XPAwarded
        }type = LevelUp;
        std::int32_t level = 0;
    };

    struct ProgressData final
    {
        float progress = 0.f;
        std::int32_t currentXP = 0;
        std::int32_t levelXP = 0;
    };

    static bool isAvailable() { return false; }
    static cro::Image getUserIcon(std::uint64_t) { return cro::Image(); }
    static void findFriends() {}
    static void inviteFriends(std::uint64_t) {}
    static void awardXP(std::int32_t) {}
    static std::int32_t getXP() { return 0; }
    static std::int32_t getLevel() { return 0; }
    static ProgressData getLevelProgress() { return {}; }
    static void storeDrivingStats(const std::array<float, 3u>&);
    static void readDrivingStats(std::array<float, 3u>&);
    static std::vector<cro::String> getLeaderboardResults(std::int32_t, std::int32_t) { return {}; }
    static void setStatus(std::int32_t, const std::vector<const char*>&) {}
    static void setGroup(std::uint64_t, std::int32_t = 0) {}
    static constexpr std::uint32_t IconSize = 64;
    static inline const std::string RSSFeed = "https://fallahn.itch.io/vga-golf/devlog.rss";
    static inline const std::string WebURL = "https://fallahn.itch.io/vga-golf";
};
