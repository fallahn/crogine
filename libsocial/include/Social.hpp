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

#include <crogine/graphics/Image.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/String.hpp>

#include <array>

//just to detect client/server version mismatch
//(terrain data changed between 100 -> 110)
//(model format changed between 120 -> 130)
//(server layout updated 140 -> 150)
//(skybox format changed 150 -> 160)
//(hole count became game rule 170 -> 180)
//(connection method changed 190 -> 1100)
//(terrain vertex data and materials changed 1100 -> 1110)
static constexpr std::uint16_t CURRENT_VER = 1120;
static const std::string StringVer("1.12.0");


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
            XPAwarded,
            OverlayActivated,
            PlayerAchievement
        }type = LevelUp;
        std::int32_t level = 0;
    };

    struct ProgressData final
    {
        float progress = 0.f;
        std::int32_t currentXP = 0;
        std::int32_t levelXP = 0;

        explicit ProgressData(std::int32_t c, std::int32_t l)
            : progress(static_cast<float>(c) / static_cast<float>(l)),
            currentXP(c),
            levelXP(l) {}
    };

    static bool isAvailable() { return false; }
    static cro::Image getUserIcon(std::uint64_t) { return cro::Image(); }
    static void findFriends() {}
    static void inviteFriends(std::uint64_t) {}
    static void awardXP(std::int32_t);
    static std::int32_t getXP();
    static std::int32_t getLevel();
    static ProgressData getLevelProgress();
    static std::uint32_t getCurrentStreak();
    static void storeDrivingStats(const std::array<float, 3u>&);
    static void readDrivingStats(std::array<float, 3u>&);
    static void insertScore(const std::string&, std::uint8_t, std::int32_t) {}
    static std::vector<cro::String> getLeaderboardResults(std::int32_t, std::int32_t) { return {}; }
    static void setStatus(std::int32_t, const std::vector<const char*>&) {}
    static void setGroup(std::uint64_t, std::int32_t = 0) {}
    static void takeScreenShot() { cro::App::getInstance().saveScreenshot(); }
    static constexpr std::uint32_t IconSize = 64;
    static inline const std::string RSSFeed = "https://fallahn.itch.io/super-video-golf/devlog.rss";
    static inline const std::string WebURL = "https://fallahn.itch.io/super-video-golf";

    enum class UnlockType
    {
        Ball, Club, Level, Generic
    };
    static std::int32_t getUnlockStatus(UnlockType);
    static void setUnlockStatus(UnlockType, std::int32_t set);

    struct UserContent final
    {
        enum
        {
            Ball, Hair, Course
        };
    };
    static std::string getBaseContentPath();
    static std::string getUserContentPath(std::int32_t);
};
