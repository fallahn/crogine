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

static const inline std::string SoapLevelFragment =
R"(
uniform sampler2D u_texture;
uniform vec4 u_uvRect;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

void main()
{
    vec2 coord = v_texCoord;

    //add distortion offset - using the colour accessors here because I can't find
    //a definitive answer whether the order is wxyz or xyzw
    float width = u_uvRect.b;
    float height = u_uvRect.a;

    float u = ((coord.x - u_uvRect.r) / width) * width;
    float v = ((coord.y - u_uvRect.g) / height) * height;

    FRAG_OUT = TEXTURE(u_texture, coord ) * v_colour;
})";
