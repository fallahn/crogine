/*-----------------------------------------------------------------------

Matt Marchant 2026
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

static inline const std::string BorderFrag =
R"(
OUTPUT

uniform sampler2D u_texture;
uniform vec4 u_croppingArea = vec4(0.0, 0.0, 192.0, 96.0);

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

const float BorderThickness = 1.0;
const vec4 Dark = vec4(0.3137,0.1569,0.1843,1.0);
const vec4 Light = vec4(0.4941,0.4275,0.2157,1.0);

void main()
{
    vec2 texSize = textureSize(u_texture, 0);
    vec2 px = floor(texSize * v_texCoord);
    vec4 colour = TEXTURE(u_texture, v_texCoord) * v_colour;

    float border = step(u_croppingArea.x + BorderThickness, px.x);
    colour = mix(Dark, colour, border);

    border = 1.0 - step((u_croppingArea.x + u_croppingArea.z) - BorderThickness, px.x);
    colour = mix(Light, colour, border);


    border = step(u_croppingArea.y + BorderThickness, px.y);
    colour = mix(Light, colour, border);

    border = 1.0 - step((u_croppingArea.y + u_croppingArea.w) - BorderThickness, px.y);
    colour = mix(Dark, colour, border);

    FRAG_OUT = colour;
})";