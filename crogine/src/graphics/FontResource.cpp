/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <crogine/graphics/FontResource.hpp>

using namespace cro;

FontResource::FontResource()
{

}

bool FontResource::load(std::uint32_t id, const std::string& path)
{
    if (m_fonts.count(id) != 0)
    {
        LogW << "Font with ID " << id << " already loaded." << std::endl;
        return false;
    }

    auto font = std::make_unique<Font>();
    if (!font->loadFromFile(path))
    {
        //load function should print error
        return false;
    }

    m_fonts.insert(std::make_pair(id, std::move(font)));
    return true;
}

//public
Font& FontResource::get(std::uint32_t id)
{
    if (m_fonts.count(id) == 0)
    {
        m_fonts.insert(std::make_pair(id, std::make_unique<Font>()));
        LogW << "Font with id " << id << " not currently loaded. Returning empty font." << std::endl;
    }
    return *m_fonts.find(id)->second;
}

void FontResource::flush()
{
    m_fonts.clear();
}