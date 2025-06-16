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

#include "../scrub/ScrubConsts.hpp" //contains font stuff

constexpr float BoxWidth = 0.8f;
constexpr float BoxHeight = 1.f;
constexpr float BoxDepth = 0.49f;

constexpr float WorldHeight = 1.2f; //this is the 3D ortho cam height

constexpr float CursorHeight = BoxHeight + 0.01f;
constexpr float OOB = CursorHeight + 0.01f;

constexpr float WheelHeight = 0.2f;
constexpr float NextBallHeight = CursorHeight - 0.1f;

namespace sb
{
    struct MessageID final
    {
        enum
        {
            CollisionMessage = cl::MessageID::Count * 300,
            GameMessage
        };
    };

    struct CollisionEvent final
    {
        cro::Entity entityA;
        cro::Entity entityB;
        std::int32_t ballID = -1;
        glm::vec3 position;

        float vol = 1.f; //estimated volume based on collision vel

        enum
        {
            //fast collision just means we want to make a noise
            Match, FastCol, Default
        }type = Match;
    };

    struct GameEvent final
    {
        enum
        {
            OutOfBounds
        }type = OutOfBounds;
    };
}