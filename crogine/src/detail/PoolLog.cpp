/*-----------------------------------------------------------------------

Matt Marchant 2025
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/detail/PoolLog.hpp>
#include <crogine/core/SysTime.hpp>

#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace
{
    std::unordered_map<std::string, std::size_t> logData;
}

using namespace cro::Detail;
void PoolLog::log(const std::string& s, std::size_t size)
{
    if (logData.count(s))
    {
        if (size > logData.at(s))
        {
            logData[s] = size;
        }
    }
    else
    {
        logData.insert(std::make_pair(s, size));
    }
}

void PoolLog::dump()
{
    if (!logData.empty())
    {
        std::ofstream file("pool.log", std::ios::app);
        if (file.is_open() && file.good())
        {
            std::vector<std::pair<std::string, std::size_t>> sortData;

            file << "----Log for " << cro::SysTime::dateString() + " " + cro::SysTime::timeString() << "----\n";
            for (const auto& [s, c] : logData)
            {
                sortData.emplace_back(std::make_pair(s, c));
            }

            std::sort(sortData.begin(), sortData.end(),
                [](const std::pair<std::string, std::size_t>& p, const std::pair<std::string, std::size_t>& q)
                {
                    return p.second > q.second;
                });

            for(const auto& [s, c] : sortData)
            {
                file << s << ": " << c << " components\n";
            }
            file << "\n\n";
        }
    }
}
