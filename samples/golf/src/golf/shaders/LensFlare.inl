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

static const inline std::string LensFlareFrag =
R"(
uniform sampler2D u_texture;
uniform sampler2D u_depthMap;
uniform vec2 u_sourcePosition = vec2(0.5);

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

#include FOG_COLOUR

void main()
{
    float depth = TEXTURE(u_depthMap, u_sourcePosition).r;
    float dist = getDistance(depth);

    vec2 offset = ((v_colour.gb * 2.0) - 1.0) * 0.02;
    float r = TEXTURE(u_texture, v_texCoord + offset).r;
    float g = TEXTURE(u_texture, v_texCoord).r;
    float b = TEXTURE(u_texture, v_texCoord - offset).r;

    FRAG_OUT.rgb = vec3(step(0.5, dist));// vec3(r,g,b) * v_colour.r * (step(0.99, dist));
    FRAG_OUT.a = 1.0;
}
)";
