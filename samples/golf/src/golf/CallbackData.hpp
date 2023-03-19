/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include <crogine/ecs/Entity.hpp>

//used to move the flag as the player approaches
struct FlagCallbackData final
{
    static constexpr float MaxHeight = 1.f;
    float targetHeight = 0.f;
    float currentHeight = 0.f;
};

//used for drone targeting
struct DroneCallbackData final
{
    float rotation = 0.f;
    float acceleration = 0.f;
    cro::Entity target;
    glm::vec3 resetPosition = glm::vec3(160.f,10.f,-100.f);
};

//used for expanding the detection radius of the sky cam
struct CamCallbackData final
{
    float progress = 0.f;
};

//used by pop-up messages
struct MessageAnim final
{
    enum
    {
        Delay, Open, Hold, Close
    }state = Delay;
    float currentTime = 0.5f;
};

//used by fastforward/skip message
struct SkipCallbackData final
{
    float progress = 0.f;
    std::uint32_t direction = 1;
    std::uint32_t buttonIndex = 0; //sets the button animation to xbox or PS
};