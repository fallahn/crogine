/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "FileHistory.hpp"

#include <algorithm>


FileHistory::FileHistory()
{

}

//public
void FileHistory::add(const std::string& item)
{
    m_history.erase(std::remove_if(m_history.begin(), m_history.end(),
        [&item](const std::string str)
        {
            return str == item;
        }), m_history.end());

    m_history.push_front(item);

    if (m_history.size() > MaxItems)
    {
        m_history.pop_back();
    }
}

const std::list<std::string>& FileHistory::getHistory() const
{
    return m_history;
}