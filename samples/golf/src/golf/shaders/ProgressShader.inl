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

#include <string>

static inline const std::string ProgressFrag =
R"(
uniform float u_progress = 0.0;

VARYING_IN vec4 v_colour;

OUTPUT

const vec4 Colour = vec4(1.0, 0.972, 0.882, 1.0);
const float TAU = 6.28318530717958647692;
const float PI = 3.14159265358979323846;

void main()
{
    vec2 coord = v_colour.rg * 2.0 - 1.0;

    float d = length(coord);
    float outer = 1.0 - smoothstep(0.9, 0.91, d);
    float inner = smoothstep(0.6, 0.61, d);

    float angle = atan(coord.y, coord.x) + PI;
    angle = 1.0 - step(u_progress * TAU, angle);

    vec4 colour = Colour;
    colour.a *= outer * inner * angle;

    FRAG_OUT = colour;
})";
