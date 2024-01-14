/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include "MonthlyChallenge.hpp"

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
//(player avatar data format changed 1110 -> 1120)
//(ball started sending wind effect 1120 -> 1124)
//(added night mode/weather 1141 -> 1150)
static constexpr std::uint16_t CURRENT_VER = 1151;
#ifdef __APPLE__
static const std::string StringVer("1.15.1 (macOS beta)");
#else
static const std::string StringVer("1.15.1");
#endif

struct HallEntry final
{
    cro::String topTenNames;
    cro::String topTenScores;
    cro::String nearestTenNames;
    cro::String nearestTenScores;
    cro::String personalBest;
    bool hasData = false;
};

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
            SocialMessage = 10000,
            StatsMessage
        };
    };

    struct StatEvent final
    {
        enum
        {
            StatsReceived,
            HOFReceived,
            AwardsReceived,
            RequestRestart
        }type = StatsReceived;
        std::int32_t index = -1;
        std::int32_t page = -1;
        std::int32_t holeCount = -1;
    };

    struct SocialEvent final
    {
        enum
        {
            AvatarDownloaded,
            LevelUp,
            XPAwarded,
            OverlayActivated,
            PlayerAchievement, //as in we should cheer, not an actual achievement
            MonthlyProgress,
            LobbyUpdated
        }type = LevelUp;
        std::int32_t level = 0; //if monthly progress then current value
        std::int32_t reason = -1; //if monthly progress then target value
        union
        {
            std::uint64_t playerID = 0;
            std::int32_t challengeID; //id of monthly challenge
        };
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
    static cro::Image userIcon;

    static cro::String getPlayerName();
    static void setPlayerName(const cro::String&);
    static bool isGreyscale() { return false; }
    static bool isValid();
    static bool isValid(const std::string&) { return true; }
    static bool isAvailable() { return false; }
    static bool isSteamdeck() { return false; }
    static cro::Image getUserIcon(std::uint64_t) { return userIcon; }
    static void findFriends() {}
    static void inviteFriends(std::uint64_t) {}
    static void awardXP(std::int32_t, std::int32_t = -1);
    static std::int32_t getXP();
    static std::int32_t getLevel();
    static std::int32_t getClubLevel();
    static ProgressData getLevelProgress();
    static std::uint32_t getCurrentStreak();
    static std::uint32_t updateStreak();
    static std::uint32_t getLongestStreak();
    static void resetProfile();
    static void storeDrivingStats(const std::array<float, 3u>&);
    static void readDrivingStats(std::array<float, 3u>&);
    static cro::String getDrivingLeader(std::int32_t holeIndex, std::int32_t tryCount) { return {}; }
    struct BoardType final
    {
        enum
        {
            DrivingRange, Courses
        };
    };
    static void findLeaderboards(std::int32_t) {}
    static void insertScore(const std::string&, std::uint8_t, std::int32_t);
    static cro::String getTopFive(const std::string& course, std::uint8_t holeCount);
    static void invalidateTopFive(const std::string& course, std::uint8_t holeCount);
    static std::int32_t getPersonalBest(const std::string&, std::uint8_t) { return -1; }
    static std::int32_t getMonthlyBest(const std::string&, std::uint8_t) { return -1; }
    static void getRandomBest() {}
    static std::vector<cro::String> getLeaderboardResults(std::int32_t, std::int32_t) { return {}; }
    static void courseComplete(const std::string&, std::uint8_t);
    static void setStatus(std::int32_t, const std::vector<const char*>&) {}
    static void setGroup(std::uint64_t, std::int32_t = 0) {}
    static void takeScreenShot() { cro::App::getInstance().saveScreenshot(); }
    static constexpr std::uint32_t IconSize = 64;
    static inline const std::string RSSFeed = "https://fallahn.itch.io/super-video-golf/devlog.rss";
    static inline const std::string WebURL = "https://fallahn.itch.io/super-video-golf";
    static void updateHallOfFame() {}
    static void refreshHallOfFame(const std::string&) {}
    static HallEntry getHallOfFame(const std::string&, std::int32_t, std::int32_t) { return {}; }
    static void refreshGlobalBoard(std::int32_t) {}
    static HallEntry getGlobalBoard(std::int32_t) { return {}; }
    static cro::String getTickerMessage() { return {}; }

    static float getCompletionCount(const std::string&, bool);

    //awards (inventory items) to display in clubhouse
    struct Award final
    {
        //dictates appearance
        enum
        {
            MonthlyGold, MonthlySilver, MonthlyBronze,
            MonthlyChallenge,
            Level10, Level20, Level30, Level40, Level50,
            LeagueFirst, LeagueSecond, LeagueThird
        };
        std::int32_t type;
        cro::String description;
    };
    static void refreshAwards();
    static const std::vector<Award>& getAwards();

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
            Ball, Hair, Course,
            Profile, Avatar
        };
    };
    static std::string getBaseContentPath();
    static std::string getUserContentPath(std::int32_t);

    static MonthlyChallenge& getMonthlyChallenge();
    static std::int32_t getMonth();

    static void showWebPage(const std::string&) {}
};
