/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <crogine/core/String.hpp>
#include <crogine/graphics/VideoPlayer.hpp>

#include <array>
#include <vector>
#include <unordered_map>

//instanciated in MenuState and pointed to by SharedStateData
//when sub-menus require it (else nullptr)
struct SharedCourseData final
{
    struct CourseData final
    {
        cro::String directory;
        cro::String title = "Untitled";
        cro::String description = "No Description";
        std::int32_t courseNumber = 0; //base 1
        std::array<cro::String, 3u> holeCount = {};
        std::vector<std::int32_t> parVals;
        bool isUser = false;
    };
    std::vector<CourseData> courseData;
    std::unordered_map<std::string, std::unique_ptr<cro::Texture>> courseThumbs;
    std::unordered_map<std::string, std::string> videoPaths;
    cro::VideoPlayer videoPlayer;
};