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

static const std::string VapourFrag =
R"(
uniform sampler2D u_diffuseMap;
uniform vec4 u_lightColour = vec4(1.0);

#include WIND_BUFFER

VARYING_IN vec2 v_texCoord0;

OUTPUT

const float BlendMultiplier = 0.5;

vec4 fetchColour(float offset, float speedMultiplier)
{
    vec4 colour = texture(u_diffuseMap, v_texCoord0 + vec2((u_windData.w * speedMultiplier) + offset, 0.0)) * BlendMultiplier;
    colour.rgb *= colour.a;

    return colour;
}

void main()
{
    vec4 colour = fetchColour(0.0, 0.01);
    colour += fetchColour(0.35, 0.012);

    float mask = texture(u_diffuseMap, v_texCoord0 + vec2(sin(u_windData.w * 0.01) * 0.01, 0.0)).a;
    colour.rgb *= (mask * mask * mask);
    colour.a = 1.0;

    FRAG_OUT = colour * u_lightColour;
})";