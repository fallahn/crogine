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

#include "CustomTournament.hpp"
#include "CommonConsts.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/detail/Assert.hpp>

namespace
{
    const std::string FileName = "selection.crs";
}

CustomTournament::CustomTournament()
    : m_title ("Untitled")
{

}

//public
void CustomTournament::load(const std::string& path)
{
    CRO_ASSERT(path.back() == '/', "");

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path + FileName, false))
    {
        for (const auto& p : cfg.getProperties())
        {
            const auto& name = p.getName();
            if (name == "title")
            {
                m_title = p.getValue<cro::String>();
            }
            else if (name == "tier_0")
            {
                m_courses[0] = p.getValue<std::string>();
            }
            else if (name == "tier_1")
            {
                m_courses[1] = p.getValue<std::string>();
            }
            else if (name == "tier_2")
            {
                m_courses[2] = p.getValue<std::string>();
            }
            else if (name == "tier_3")
            {
                m_courses[3] = p.getValue<std::string>();
            }
        }
    }

    if (m_title.size() > ConstVal::MaxStringChars)
    {
        m_title = m_title.substr(0, ConstVal::MaxStringChars);
    }
    else if (m_title.empty())
    {
        m_title = "Untitled";
    }

    //replace any invalid strings with something semi-sane
    for (auto& c : m_courses)
    {
        if (c.find("course_") == std::string::npos)
        {
            c = "course_01";
        }
    }
}

void CustomTournament::save(const std::string& path)
{
    cro::ConfigFile cfg("course_list");
    cfg.addProperty("title").setValue(m_title);

    for (auto i = 0; i < 4; ++i)
    {
        cfg.addProperty("tier_" + std::to_string(i)).setValue(m_courses[i]);
    }

    cfg.save(path + FileName);
}

void CustomTournament::setCourse(std::size_t idx, const std::string& course)
{
    if (idx < m_courses.size())
    {
        m_courses[idx] = course;
    }
}