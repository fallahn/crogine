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

static const inline std::string BoxBlurFrag = 
R"(
uniform sampler2D u_texture;

VARYING_IN vec4 v_colour;
VARYING_IN vec2 u_texCoord;

OUTPUT

const int size = 3;
const float separation = 1.0;

void main()
{
    float count = 0.0;
    ivec2 texSize = textureSize(u_texture, 0);

    for (int i = -size; i <= size; ++i)
    {
        for (int j = -size; j <= size; ++j)
        {
            FRAG_OUT.rgb += TEXTURE(u_texture, (gl_FragCoord.xy + (vec2(i, j) * separation))  / texSize).rgb;
            count += 1.0;
        }
    }

    FRAG_OUT.rgb /= count;
}

)";