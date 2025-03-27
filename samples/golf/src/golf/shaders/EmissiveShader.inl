/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

//Used with CelVertexShader

#include <string>

static const inline std::string EmissiveFragment =
R"(

VARYING_IN vec3 v_normal;
VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_worldPosition;

#define USE_MRT
#include OUTPUT_LOCATION

void main()
{
    FRAG_OUT = v_colour;
    LIGHT_OUT = v_colour;
    NORM_OUT = vec4(normalize(v_normal), 0.0);
    POS_OUT = vec4(v_worldPosition, 1.0);
})";
