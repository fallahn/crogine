/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

inline const std::string WireframeVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;

//#if defined (DASHED)
    ATTRIBUTE vec2 a_texCoord0;
//#endif

    uniform mat4 u_worldMatrix;
    uniform mat4 u_worldViewMatrix;
    uniform mat4 u_projectionMatrix;
    uniform vec4 u_clipPlane;

#if defined (CULLED)
    uniform HIGH vec3 u_cameraWorldPosition;

    #include RESOLUTION_BUFFER
#endif

#if defined (HUE)
    uniform float u_colourRotation = 1.0;
    uniform vec4 u_colour = vec4(1.0);

    vec3 rgb2hsv(vec3 c)
    {
        vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
        vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
        vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

        float d = q.x - min(q.w, q.y);
        float e = 1.0e-10;
        return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
    }

    vec3 hsv2rgb(vec3 c)
    {
        vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }
#endif

    VARYING_OUT LOW vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;
        gl_Position = wvp * position;

        vec4 worldPos = u_worldMatrix * position;

        v_colour = a_colour;
#if defined (DASHED)
        v_texCoord = a_texCoord0;
#endif

#if defined (CULLED)
        vec3 distance = worldPos.xyz - u_cameraWorldPosition;

//float near = 3.5;
//float far = 4.0;

float near = 10.0;
float far = 15.0;

        v_colour.a *= smoothstep(near*near, far*far, dot(distance, distance));
#endif

#if defined (HUE)
        vec3 hsv = rgb2hsv(a_colour.rgb);
        hsv.x += u_colourRotation;
        v_colour.rgb = hsv2rgb(hsv) * u_colour.rgb;
#endif

        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    }
)";

inline const std::string WireframeFragment = R"(
#include OUTPUT_LOCATION
    uniform vec4 u_colour = vec4(1.0);
#if defined (DASHED)
    uniform float u_time;
#endif
    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    const float TAU = 6.28;

    void main()
    {
        vec4 colour = v_colour * u_colour;
        colour.rgb += (1.0 - colour.a) * colour.rgb;

#if defined (DASHED)
        float alpha = (sin(u_time - ((v_texCoord.x * TAU) * 20.0)) + 1.0) * 0.5;
        alpha = step(0.5, alpha);
        if (alpha < 0.5) discard;
#endif
        FRAG_OUT = colour;

#if defined (USE_MRT)
        NORM_OUT = vec4(0.0);
        LIGHT_OUT = vec4(vec3(0.0), 1.0);
#endif
    }
)";