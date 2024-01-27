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

Based on articles: http://www.extentofthejam.com/pseudo/
                   https://codeincomplete.com/articles/javascript-racer/

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>

struct Player final
{
    //float x = 0.f; //+/- 1 from X centre
    //float y = 0.f;
    //float z = 0.f; //rel distance from camera
    glm::vec3 position = glm::vec3(0.f);
    float speed = 0.f;

    bool offTrack = false;

    static inline constexpr float MaxSpeed = SegmentLength * 120.f; //60 is our frame time
    static inline constexpr float Acceleration = MaxSpeed / 3.f;
    static inline constexpr float Braking = -MaxSpeed;
    static inline constexpr float Deceleration = -Acceleration;
    static inline constexpr float OffroadDeceleration = -MaxSpeed / 2.f;
    static inline constexpr float OffroadMaxSpeed = MaxSpeed / 4.f;
    static inline constexpr float Centrifuge = 530.f; // "pull" on cornering

    struct Model final
    {
        float targetRotationX = 0.f;
        float rotationX = 0.f;
        float rotationY = 0.f;

        static constexpr float MaxX = 0.1f;
        static constexpr float MaxY = 0.2f;
    }model;

    struct State final
    {
        enum
        {
            Normal, Reset
        };
    };
    std::int32_t state = 0;
};
