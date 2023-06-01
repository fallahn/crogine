/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
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

#include "sqlite3.h"

#include <string>
#include <array>
#include <vector>
#include <limits>

struct CourseRecord final
{
    std::array<std::int32_t, 18u> holeScores = {};
    std::int32_t total = 0;
    std::int32_t totalPar = 0;
    std::int32_t courseIndex = 0;
    std::int32_t holeCount = 0; //all, front, back
    std::int32_t wasCPU = 0; //bool
    std::uint64_t timestamp = 0u;
};

struct PersonalBestRecord final
{
    std::int32_t hole = 0;
    std::int32_t course = 0;
    float longestDrive = 0.f;
    float longestPutt = 0.f;
    std::int32_t score = 12;
};

class ProfileDB final
{
public:
    ProfileDB();
    ~ProfileDB(); //RAII closes DB if open

    ProfileDB(const ProfileDB&) = delete;
    ProfileDB(ProfileDB&&) = delete;
    ProfileDB& operator = (const ProfileDB&) = delete;
    ProfileDB& operator = (ProfileDB&&) = delete;

    //opens the DB at the given path, returns false on failure
    bool open(const std::string& path);

    //attempts to insert the record into the db
    //creates a table for the hole ID if it doesn't exist
    //returns false if DB isn't open or creating record fails
    bool insertCourseRecord(const CourseRecord&);

    //returns the requested number of records, or as many exist
    std::vector<CourseRecord> getCourseRecords(std::int32_t holeIndex, std::int32_t recordCount = std::numeric_limits<std::int32_t>::max());
    std::int32_t getCourseRecordCount(std::int32_t courseIndex, std::int32_t holeCount) const;

    bool insertPersonalBestRecord(const PersonalBestRecord&);
    //returns the personal bests for the given course, sorted by hole number
    std::vector<PersonalBestRecord> getPersonalBest(std::int32_t courseIndex) const;

private:
    sqlite3* m_connection;
    
    std::array<std::vector<std::int32_t>, 3u> m_courseRecordCounts;

    //creates a new table for the given course ID if it doesn't exist
    bool createCourseTable(std::int32_t id);
    void createPersonalBestTable();

    //fetches the record count for the given course index
    void fetchRecordCount(std::int32_t courseIndex);
};