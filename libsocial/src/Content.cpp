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

#include <Content.hpp>

#include <crogine/core/App.hpp>

#include <cassert>

namespace
{
    const std::array SearchPaths =
    {
        std::string("dlc/adventurer/")
    };
}

std::vector<std::string> Content::getInstallPaths()
{
    std::vector<std::string> ret = { "assets/golf/" };

    for (const auto& path : SearchPaths)
    {
        if (cro::FileSystem::directoryExists(path))
        {
            ret.push_back(path);
        }
    }

    return ret;
}

std::string Content::getBaseContentPath()
{
    return cro::App::getPreferencePath() + "user/1234/";
}

std::string Content::getUserContentPath(std::int32_t contentType)
{
    switch (contentType)
    {
    default:
        assert(false);
        return "";
    case Content::UserContent::Ball:
        return getBaseContentPath() + "balls/";
    case Content::UserContent::Course:
        return getBaseContentPath() + "course/";
    case Content::UserContent::Hair:
        return getBaseContentPath() + "hair/";
    case Content::UserContent::Profile:
        return getBaseContentPath() + "profiles/";
    case Content::UserContent::Career:
        return getBaseContentPath() + "career/";
    case Content::UserContent::Avatar:
        return getBaseContentPath() + "avatars/";
    case Content::UserContent::Flag:
        return getBaseContentPath() + "flags/";
    case Content::UserContent::Clubs:
        return getBaseContentPath() + "clubs/";
    case Content::UserContent::Voice:
        return getBaseContentPath() + "voice/";
    }
}
