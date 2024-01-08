/*-----------------------------------------------------------------------

Matt Marchant 2022
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

inline const std::string BeaconVertex = R"(
#line 1
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform vec4 u_clipPlane;
    uniform float u_colourRotation = 1.0;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;

#include HSV

    void main()
    {
#if defined (SPRITE)
        vec4 position = vec4(a_position.xy, 0.0, 1.0);
#else
        vec4 position = a_position;
#endif

        gl_Position = u_viewProjectionMatrix * u_worldMatrix * position;

        v_texCoord = a_texCoord0;

        vec3 hsv = rgb2hsv(a_colour.rgb);
        hsv.x += u_colourRotation;
        v_colour = vec4(hsv2rgb(hsv), a_colour.a);

        vec4 worldPos = u_worldMatrix * position;
        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    })";


inline const std::string BeaconFragment = R"(
#include OUTPUT_LOCATION

    uniform sampler2D u_diffuseMap;
    uniform vec4 u_colour;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    void main()
    {
#if defined (TEXTURED)
        FRAG_OUT = TEXTURE(u_diffuseMap, v_texCoord) * v_colour * u_colour;
#else
        FRAG_OUT = v_colour;
#endif

#if defined (USE_MRT)
        NORM_OUT = vec4(0.0); //masks off light map

#if defined(INTENSITY)
        LIGHT_OUT = FRAG_OUT;
        FRAG_OUT.rgb *= INTENSITY;
#else
        LIGHT_OUT = vec4(FRAG_OUT.rgb * FRAG_OUT.a, 1.0);
#endif
#endif

    })";