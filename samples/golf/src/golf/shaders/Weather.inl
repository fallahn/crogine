/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

static const inline std::string WeatherVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_worldViewMatrix;
    uniform mat4 u_projectionMatrix;
    uniform vec4 u_clipPlane;

    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };

    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };

#if defined (CULLED)
    uniform HIGH vec3 u_cameraWorldPosition;
#endif

    VARYING_OUT LOW vec4 v_colour;

#if defined(SYSTEM_HEIGHT)
    const float SystemHeight = SYSTEM_HEIGHT;
#else
    const float SystemHeight = 80.0;
#endif

#if defined(EASE_SNOW)
    const float PI = 3.1412;
    float ease(float i)
    {
        return sin((i * PI) / 2.0);
    }

    const float FallSpeed = 1.0;
    const float WindEffect = 40.0;
#else
    float ease(float i)
    {
        //return sqrt(1.0 - pow(i - 1.0, 2.0));
        return i;
    }
    const float FallSpeed = 32.0;//16.0;
    const float WindEffect = 10.0;
#endif

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;

        float p = position.y - (u_windData.w * FallSpeed);
        p = mod(p, SystemHeight);
        //p = ease(0.2 + ((p / SystemHeight) * 0.8));
        p = ease((p / SystemHeight));

        position.y  = p * SystemHeight;

        position.x -= p * u_windData.x * u_windData.y * WindEffect;
        position.z -= p * u_windData.z * u_windData.y * WindEffect;


        gl_Position = wvp * position;
        gl_PointSize = u_projectionMatrix[1][1] / gl_Position.w * 10.0 * u_pixelScale * (FallSpeed);

        vec4 worldPos = u_worldMatrix * position;
        v_colour = a_colour;

#if defined (CULLED)
        vec3 distance = worldPos.xyz - u_cameraWorldPosition;
        v_colour.a *= smoothstep(12.25, 16.0, dot(distance, distance));
#endif

        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    }
)";

static const inline std::string RainFragment = R"(
    #define USE_MRT
    #include OUTPUT_LOCATION

    uniform vec4 u_colour = vec4(1.0);

    VARYING_IN vec4 v_colour;

    float ease(float i)
    {
        //return sqrt(pow(i, 2.0));
        return pow(i, 5.0);
    }

    void main()
    {
        vec4 colour = v_colour * u_colour;
        
        //if alpha blended
        //colour.rgb += (1.0 - colour.a) * colour.rgb;
        colour.a *= 0.5 + (0.5 * ease(gl_PointCoord.y));

        float crop = step(0.495, gl_PointCoord.x);
        crop *= 1.0 - step(0.505, gl_PointCoord.x);

        //if(crop < 0.05) discard;

        colour.a *= u_colour.a * crop;
        FRAG_OUT = colour;

        LIGHT_OUT = vec4(vec3(0.0), colour.a);
    }
)";

static const inline std::string FireworkVert = R"(
ATTRIBUTE vec4 a_position;

#include WVP_UNIFORMS
uniform mat4 u_viewMatrix;

uniform float u_size = 1.0;
uniform float u_progress = 0.0;

layout (std140) uniform PixelScale
{
    float u_pixelScale;
};

#if !defined(GRAVITY)
#define GRAVITY 0.06
#endif

const float Gravity = GRAVITY;
#if !defined(POINT_SIZE)
#define POINT_SIZE 15.0
#endif
const float PointSize = POINT_SIZE;

void main()
{
    vec4 position = u_worldMatrix * a_position;
    position.y -= (Gravity * u_progress) * u_progress;

    gl_Position = u_projectionMatrix * u_viewMatrix * position;
    
    gl_PointSize = (0.2 + (0.8 * u_progress)) * PointSize * u_size * u_pixelScale * (u_projectionMatrix[1][1] / gl_Position.w);
})";


static const inline std::string FireworkFragment = R"(

uniform float u_progress = 0.0;
uniform vec4 u_colour = vec4(1.0);

OUTPUT

const float stepPos = (0.499 * 0.499);

void main()
{
    vec2 coord = gl_PointCoord - vec2(0.5);
    float len2 = dot(coord, coord);
#if !defined(POINT_SIZE) //for some reason we have to do this differently on the menu
    FRAG_OUT = vec4(u_colour.rgb * pow(2.0, -8.0 * u_progress) * (1.0 - step(stepPos, len2)), 1.0);
#else
    FRAG_OUT = vec4(u_colour.rgb * (1.0 - smoothstep(0.6, 0.998, u_progress)) * (1.0 - step(stepPos, len2)), 1.0);
#endif
//FRAG_OUT = vec4(gl_PointCoord, 1.0, 1.0);

})";