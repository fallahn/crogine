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

#include "ProfileDB.hpp"
#include "../golf/HoleData.hpp"

#include <crogine/core/FileSystem.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/detail/Assert.hpp>

namespace
{
    constexpr std::int32_t MinCourse = 0;
    constexpr std::int32_t MaxCourse = 11;
}

ProfileDB::ProfileDB()
    : m_connection(nullptr)
{
    for (auto& v : m_courseRecordCounts)
    {
        v.resize(MaxCourse + 1);
        std::fill(v.begin(), v.end(), 0);
    }
}

ProfileDB::~ProfileDB()
{
    //close any open connection
    if (m_connection)
    {
        //TODO we should also finalise pending
        //queries - must check how we know if there
        //are any outstanding or if it's just ok
        //to call finalise regardless.

        sqlite3_close(m_connection);
    }
}

//public
bool ProfileDB::open(const std::string& path)
{
    if (m_connection)
    {
        //TODO finalise any pending actions
        sqlite3_close(m_connection);
        m_connection = nullptr;
    }

    auto result = sqlite3_open(path.c_str(), &m_connection);
    if (result != SQLITE_OK)
    {
        LogE << sqlite3_errmsg(m_connection) << std::endl;
        sqlite3_close(m_connection);
        m_connection = nullptr;

        return false;
    }

    //if the layout is not yet created, do so
    for (auto i = 0; i <= MaxCourse; ++i)
    {
        createCourseTable(i);
        fetchRecordCount(i);
    }
    createPersonalBestTable();

    return true;
}

bool ProfileDB::insertCourseRecord(const CourseRecord& record)
{
    CRO_ASSERT(record.courseIndex >= MinCourse && record.courseIndex <= MaxCourse, "");
    if (record.courseIndex < MinCourse || record.courseIndex > MaxCourse)
    {
        LogE << "ProfileDB (INSERT) - " << record.courseIndex << ": out of range course index" << std::endl;
        return false;
    }
    
    if (m_connection == nullptr)
    {
        LogE << "ProfileDB (INSERT) - Could not insert record, DB is not open" << std::endl;
        return false;
    }

    auto ts = std::to_string(cro::SysTime::epoch());
    std::string holeVals;
    for (auto h : record.holeScores)
    {
        holeVals += std::to_string(h) + ",";
    }

    std::string query = "INSERT INTO " + CourseNames[record.courseIndex]
        + "(H1,H2,H3,H4,H5,H6,H7,H8,H9,H10,H11,H12,H13,H14,H15,H16,H17,H18,Total,TotalPar,Count,Date,WasCPU)"
        + "VALUES("
        + holeVals + std::to_string(record.total) + "," + std::to_string(record.totalPar) + ","
        + std::to_string(record.holeCount) + "," + ts + "," + std::to_string(record.wasCPU)
        + ")";

    sqlite3_stmt* out = nullptr;
    int result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
    if (result != SQLITE_OK)
    {
        LogE << sqlite3_errmsg(m_connection) << std::endl;
        sqlite3_finalize(out);
        return false;
    }

    sqlite3_step(out);
    sqlite3_finalize(out);

    m_courseRecordCounts[record.holeCount][record.courseIndex]++;

    return true;
}

std::vector<CourseRecord> ProfileDB::getCourseRecords(std::int32_t courseIndex, std::uint64_t oldestTimeStamp, bool getCPU, std::int32_t recordCount)
{
    CRO_ASSERT(courseIndex >= MinCourse && courseIndex <= MaxCourse, "");
    if (courseIndex < MinCourse || courseIndex > MaxCourse)
    {
        LogE << "ProfileDB (SELECT) - " << courseIndex << ": out of range course index" << std::endl;
        return {};
    }

    if (m_connection == nullptr)
    {
        LogE << "ProfileDB (SELECT) - Could not fetch records, database not open" << std::endl;
        return {};
    }

    std::vector<CourseRecord> retVal;
    std::string oldestRecord = std::to_string(oldestTimeStamp);

    std::string query;
    if (getCPU)
    {
        query = "SELECT * FROM " + CourseNames[courseIndex] + " WHERE Date >= " + oldestRecord + " ORDER BY Date DESC";
    }
    else
    {
        query = "SELECT * FROM " + CourseNames[courseIndex] + " WHERE Date >= " + oldestRecord + " AND wasCPU = 0 ORDER BY Date DESC";
    }
    sqlite3_stmt* out = nullptr;
    int result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
    if (result != SQLITE_OK)
    {
        LogE << sqlite3_errmsg(m_connection) << std::endl;
        sqlite3_finalize(out);
        return retVal;
    }


    do
    {
        result = sqlite3_step(out);
        
        if (result == SQLITE_ROW)
        {
            auto& record = retVal.emplace_back();
            
            for (auto i = 0; i < 18; ++i)
            {
                record.holeScores[i] = sqlite3_column_int(out, i);
            }
            record.total = sqlite3_column_int(out, 18);
            record.totalPar = sqlite3_column_int(out, 19);
            record.holeCount = sqlite3_column_int(out, 20);
            record.timestamp = sqlite3_column_int64(out, 21);
            record.wasCPU = sqlite3_column_int(out, 22);
        }

    } while (result == SQLITE_ROW && recordCount-- != 0);

    sqlite3_finalize(out);

    return retVal;
}

std::int32_t ProfileDB::getCourseRecordCount(std::int32_t index, std::int32_t holeCount) const
{
    CRO_ASSERT(index < MaxCourse + 1, "");
    CRO_ASSERT(holeCount < 3, "");
    return m_courseRecordCounts[holeCount][index];
}

bool ProfileDB::insertPersonalBestRecord(const PersonalBestRecord& record)
{
    if (record.hole > 17 || record.course > MaxCourse)
    {
        LogE << "Could not insert personal best - hole or course index out of range." << std::endl;
        return false;
    }

    if (m_connection == nullptr)
    {
        LogE << "Could not insert personal best - database not open" << std::endl;
        return false;
    }

    auto newRecord = record;
    std::string query = "SELECT * FROM PERSONAL_BEST WHERE Hole = " 
        + std::to_string(record.hole) 
        + " AND Course = " + std::to_string(record.course)
        + " AND PuttAssist = " + std::to_string(record.wasPuttAssist);

    sqlite3_stmt* out = nullptr;
    int result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
    if (result != SQLITE_OK)
    {
        LogE << sqlite3_errmsg(m_connection) << std::endl;
        sqlite3_finalize(out);
        return false;
    }


    if (result = sqlite3_step(out); result == SQLITE_ROW)
    {
        newRecord.longestDrive = static_cast<float>(sqlite3_column_double(out, 2));
        newRecord.longestPutt = static_cast<float>(sqlite3_column_double(out, 3));
        newRecord.score = sqlite3_column_int(out, 4);

        bool updated = false;
        if (newRecord.longestDrive < record.longestDrive)
        {
            newRecord.longestDrive = record.longestDrive;
            updated = true;
        }
        if (newRecord.longestPutt < record.longestPutt)
        {
            newRecord.longestPutt = record.longestPutt;
            updated = true;
        }
        if (newRecord.score > record.score)
        {
            newRecord.score = record.score;
            updated = true;
        }

        sqlite3_finalize(out);
        out = nullptr;

        if (updated)
        {
            //update entry
            query = "UPDATE PERSONAL_BEST SET LongestDrive = " + std::to_string(newRecord.longestDrive)
                + ", LongestPutt = " + std::to_string(newRecord.longestPutt)
                + ", Score = " + std::to_string(newRecord.score)
                + " WHERE Hole = " + std::to_string(record.hole) 
                + " AND Course = " + std::to_string(record.course)
                + " AND PuttAssist = " + std::to_string(record.wasPuttAssist);

            result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
            if (result != SQLITE_OK)
            {
                LogE << sqlite3_errmsg(m_connection) << std::endl;
                sqlite3_finalize(out);
                return false;
            }
            sqlite3_step(out);
            sqlite3_finalize(out);
        }
    }
    else
    {
        //insert entry
        sqlite3_finalize(out);
        out = nullptr;

        query = "INSERT INTO PERSONAL_BEST (Hole, Course, LongestDrive, LongestPutt, Score, PuttAssist) VALUES ("
            + std::to_string(record.hole) + ", "
            + std::to_string(record.course) + ", "
            + std::to_string(record.longestDrive) + ", "
            + std::to_string(record.longestPutt) + ", "
            + std::to_string(record.score) + ", "
            + std::to_string(record.wasPuttAssist) + ")";


        result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
        if (result != SQLITE_OK)
        {
            LogE << sqlite3_errmsg(m_connection) << std::endl;
            sqlite3_finalize(out);
            return false;
        }
        sqlite3_step(out);
        sqlite3_finalize(out);
    }

    return true;
}

std::vector<PersonalBestRecord> ProfileDB::getPersonalBest(std::int32_t courseIndex) const
{
    CRO_ASSERT(courseIndex >= MinCourse && courseIndex <= MaxCourse, "");
    if (courseIndex < MinCourse || courseIndex > MaxCourse)
    {
        LogE << "ProfileDB (SELECT PB) - " << courseIndex << ": out of range course index" << std::endl;
        return {};
    }

    if (m_connection == nullptr)
    {
        LogE << "ProfileDB (SELECT PB) - Could not fetch records, database not open" << std::endl;
        return {};
    }

    std::vector<PersonalBestRecord> retVal;
    std::string query = "SELECT * FROM PERSONAL_BEST WHERE Course = " + std::to_string(courseIndex) + " ORDER BY Hole";

    sqlite3_stmt* out = nullptr;
    int result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
    if (result != SQLITE_OK)
    {
        LogE << sqlite3_errmsg(m_connection) << std::endl;
        sqlite3_finalize(out);
        return retVal;
    }

    do
    {
        result = sqlite3_step(out);

        if (result == SQLITE_ROW)
        {
            auto& record = retVal.emplace_back();

            record.hole = sqlite3_column_int(out, 0);
            record.course = sqlite3_column_int(out, 1);
            record.longestDrive = static_cast<float>(sqlite3_column_double(out, 2));
            record.longestPutt = static_cast<float>(sqlite3_column_double(out, 3));
            record.score = sqlite3_column_int(out, 4);
            record.wasPuttAssist = sqlite3_column_int(out, 5);
        }

    } while (result == SQLITE_ROW);

    sqlite3_finalize(out);

    return retVal;
}

//private
bool ProfileDB::createCourseTable(std::int32_t index)
{
    /*
    H1 INT - H18 INTEGER, Total INTEGER, TotalPar INTEGER, Count INTEGER, Date INTEGER WasCPU INTEGER
    */

    std::string query = "CREATE TABLE IF NOT EXISTS " + CourseNames[index] 
        + " (H1 INTEGER, H2 INTEGER, H3 INTEGER, H4 INTEGER, H5 INTEGER, H6 INTEGER, H7 INTEGER, H8 INTEGER, H9 INTEGER, "
        + "H10 INTEGER, H11 INTEGER, H12 INTEGER, H13 INTEGER, H14 INTEGER, H15 INTEGER, H16 INTEGER, H17 INTEGER, H18 INTEGER, "
        + "Total INTEGER, TotalPar INTEGER, Count INTEGER, Date INTEGER, WasCPU INTEGER)";

    sqlite3_stmt* out = nullptr;
    int result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
    if (result != SQLITE_OK)
    {
        LogE << sqlite3_errmsg(m_connection) << std::endl;
        sqlite3_finalize(out);
        return false;
    }

    sqlite3_step(out);
    sqlite3_finalize(out);

    return true;
}

void ProfileDB::createPersonalBestTable()
{
    std::string query = "CREATE TABLE IF NOT EXISTS PERSONAL_BEST (Hole INTEGER, Course INTEGER, LongestDrive REAL, LongestPutt REAL, Score INTEGER, PuttAssist INTEGER)";

    sqlite3_stmt* out = nullptr;
    int result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
    if (result != SQLITE_OK)
    {
        LogE << sqlite3_errmsg(m_connection) << std::endl;
        sqlite3_finalize(out);
        return;
    }

    sqlite3_step(out);
    sqlite3_finalize(out);
}

void ProfileDB::fetchRecordCount(std::int32_t courseIndex)
{
    for (auto i = 0; i < 3; ++i)
    {
        CRO_ASSERT(courseIndex < MaxCourse + 1, "");
        std::string query = "SELECT COUNT(*) FROM " + CourseNames[courseIndex] + " WHERE Count = " + std::to_string(i);

        sqlite3_stmt* out = nullptr;
        int result = sqlite3_prepare_v2(m_connection, query.c_str(), -1, &out, nullptr);
        if (result != SQLITE_OK)
        {
            LogE << sqlite3_errmsg(m_connection) << std::endl;
            sqlite3_finalize(out);
            continue;
        }

        result = sqlite3_step(out);
        if (result == SQLITE_ROW)
        {
            m_courseRecordCounts[i][courseIndex] = sqlite3_column_int(out, 0);
        }


        sqlite3_finalize(out);
    }
}