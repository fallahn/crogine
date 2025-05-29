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

#pragma once

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/gui/Gui.hpp>

#include <string>
#include <vector>

namespace pg
{
    struct Item final
    {
        enum
        {
            Title, Text, 
            Image, Separator,
            Header
        }type = Text;
        std::basic_string<std::uint8_t> string;
        const cro::Texture* image = nullptr;
        glm::vec2 frameSize = glm::vec2(0.f);

        struct Animation final
        {
            glm::vec2 frameSizeNorm = glm::vec2(0.f);
            glm::ivec2 frameCount = glm::ivec2(0);
            std::int32_t currentFrame = 0;

            float currentTime = 0.f;
            float FPS = 1.f / 14.f;
            bool active = false;

            cro::FloatRect update()
            {
                static constexpr float timeStep = 1.f / 60.f;
                currentTime += timeStep;
                if (currentTime > FPS)
                {
                    currentTime -= FPS;
                    currentFrame++;
                    currentFrame %= frameCount.x * frameCount.y;
                }
                
                const auto x = currentFrame % frameCount.x;
                const auto y = currentFrame / frameCount.x;

                glm::vec2 pos(x * frameSizeNorm.x, y * frameSizeNorm.y);

                return { pos, pos + frameSizeNorm };
            }
        }animation;
    };

    struct Chapter final
    {
        //I know this duplicates, but the item list
        //resizing invalidates a pointer to the string
        std::basic_string<std::uint8_t> title;
        std::vector<Item> items;
        bool isVisible = false;
    };
}
