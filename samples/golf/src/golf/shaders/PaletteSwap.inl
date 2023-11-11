/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

/*
Palette swap technique based on https://gamedev.stackexchange.com/questions/80577/palette-reduction-to-pre-defined-palette
*/


#include <string>

inline const std::string PaletteSwapVertex = 
R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;

ATTRIBUTE vec2 a_position;
ATTRIBUTE MED vec2 a_texCoord0;
ATTRIBUTE LOW vec4 a_colour;

VARYING_OUT LOW vec4 v_colour;
VARYING_OUT MED vec2 v_texCoord;

void main()
{
    gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
    v_colour = a_colour;
    v_texCoord = a_texCoord0;
})";

inline const std::string PaletteSwapFragment = 
R"(
OUTPUT

uniform sampler2D u_texture;
uniform sampler2D u_palette;

VARYING_IN vec4 v_colour;
VARYING_IN vec2 v_texCoord;

void main()
{
    vec3 outColour = TEXTURE(u_texture, v_texCoord).rgb * 255.0;

    outColour = floor(outColour * 16.0);
    float index = outColour.r + (outColour.g * 16.0) + (outColour.b * 16.0 * 16.0);

    vec2 paletteCoord = vec2(index / 64.0, 64.0 - mod(index, 64.0));

    FRAG_OUT = TEXTURE(u_palette, floor(paletteCoord) / vec2(64.0)) * v_colour;
})";