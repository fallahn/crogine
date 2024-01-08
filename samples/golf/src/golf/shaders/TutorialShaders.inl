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

#include <string>

inline const std::string TutorialVertexShader = R"(
        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        ATTRIBUTE vec2 a_position;
        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE LOW vec4 a_colour;

        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT MED vec2 v_position;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_colour = a_colour;
            v_position = a_position;
        })";

inline const std::string TutorialSlopeShader =
R"(
    OUTPUT

    uniform float u_time;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_position;

    void main()
    {
        float alpha = (sin((v_position.x * 0.5) - (u_time * 0.1)) + 1.0) * 0.5;
        alpha = step(0.15, alpha);

        FRAG_OUT = mix(vec4(1.0, 1.0, 0.0, 1.0), v_colour, alpha);
        //FRAG_OUT.a *= alpha;
    }
)";
