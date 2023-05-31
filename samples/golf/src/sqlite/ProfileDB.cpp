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

#include <crogine/core/FileSystem.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/detail/Assert.hpp>

namespace
{
    const std::array<std::string, 10u> CourseNames =
    {
        "course_01",
        "course_02",
        "course_03",
        "course_04",
        "course_05",
        "course_06",
        "course_07",
        "course_08",
        "course_09",
        "course_10",
    };
    constexpr std::int32_t MinCourse = 0;
    constexpr std::int32_t MaxCourse = 9;
}

ProfileDB::ProfileDB()
    : m_connection(nullptr)
{

}

ProfileDB::~ProfileDB()
{
    //close any open connection
    if (m_connection)
    {
        //TODO we should also finalise pending
        //queries - must check how we know if there
        //are any outstandinf or if it's just ok
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
        createTable(i);
    }


    return true;
}

bool ProfileDB::insertRecord(const ProfileRecord& record)
{
    if (m_connection == nullptr)
    {
        LogE << "ProfileDB - Could not insert record, DB is not open" << std::endl;
        return false;
    }

    auto ts = cro::SysTime::epoch();
    return false;
}

std::vector<ProfileRecord> ProfileDB::getRecords(std::int32_t courseIndex, std::int32_t recordCount)
{
    CRO_ASSERT(courseIndex >= MinCourse && courseIndex <= MaxCourse, "");
    if (courseIndex < MinCourse || courseIndex > MaxCourse)
    {
        LogE << "ProfileDB - " << courseIndex << ": out of range course index" << std::endl;
        return {};
    }

    if (m_connection == nullptr)
    {
        LogE << "ProfileDB - Could not fetch records, database not open" << std::endl;
        return {};
    }


    return {};
}

//private
bool ProfileDB::createTable(std::int32_t index)
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