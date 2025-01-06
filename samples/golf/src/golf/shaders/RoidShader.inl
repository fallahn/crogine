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

static inline const std::string RoidFrag =
R"(
uniform sampler2D u_diffuseMap;
uniform float u_texOffset;

VARYING_IN vec2 v_texCoord0;

OUTPUT

void main()
{
    float fade = 1.0 - smoothstep(0.39, 0.49, v_texCoord0.x);
    fade *= smoothstep(0.01, 0.21, v_texCoord0.x);

    FRAG_OUT = TEXTURE(u_diffuseMap, v_texCoord0 + vec2(u_texOffset, 0.0)) * fade;
    FRAG_OUT.a = 1.0;
})";
