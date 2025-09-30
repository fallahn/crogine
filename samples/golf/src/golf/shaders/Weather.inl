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

#include <string>

static inline const std::string MoonFrag =
R"(
//we can use compile time constants instead of uniforms

#ifndef ROTATION
#define ROTATION u_rotation
#endif

#ifndef DIRECTION
#define DIRECTION u_lightDir
#endif

uniform sampler2D u_diffuseMap;

uniform mat2 u_rotation = mat2(1.0);
uniform vec3 u_lightDir = vec3(0.0, 0.0, 1.0);

VARYING_IN vec2 v_texCoord0;
VARYING_IN vec4 v_colour;

OUTPUT

vec3 sphericalNormal(vec2 coord)
{
#if defined(MOON)
//hack cos moon png is 1/4 size actual texture
coord *= 2.0;
coord -= 0.5;
#endif

    coord = clamp(coord, 0.0, 1.0);

    float dx  = ( coord.x - 0.5 ) * 2.0;
    float dy  = ( coord.y - 0.5 ) * 2.0;
    float distSqr = (dx * dx) + (dy * dy);
         
    return normalize(vec3(dx, dy, 1.0 - distSqr));
}

const float Levels = 20.0;
const vec3 skyColour = vec3(0.0039, 0.0353, 0.1059);
const vec3 WaterColour = vec3(0.02, 0.078, 0.578);

void main()
{
    vec2 coord = v_texCoord0 - 0.5;
    coord = ROTATION * coord;
    coord += 0.5;

    vec4 colour = TEXTURE(u_diffuseMap, coord);

    vec3 normal = sphericalNormal(coord);
    float amount = smoothstep(-0.1, 0.1, dot(normal, DIRECTION));

    amount *= Levels;
    amount = floor(amount);
    amount /= Levels;

    amount = 0.1 + (0.9 * amount);

    if(colour.a < 0.35) discard;

    colour.rgb = mix(skyColour, colour.rgb, amount);
    FRAG_OUT = vec4(mix(WaterColour, colour.rgb, v_colour.g), 1.0);
})";

static const inline std::string UmbrellaFrag = R"(
#define USE_MRT
#include OUTPUT_LOCATION
#include HSV
#include BAYER_MATRIX
#include LIGHT_UBO
#include LIGHT_COLOUR

VARYING_IN vec3 v_worldPosition;
VARYING_IN float v_ditherAmount;
VARYING_IN vec3 v_normal;
VARYING_IN vec4 v_colour;
flat in int v_instanceID;

void main()
{
    vec3 colour = getLightColour().rgb;

    vec3 c = rgb2hsv(v_colour.rgb);
    c.r += 0.15 * v_instanceID;
    c = hsv2rgb(c);

    colour *= c;


#define COLOUR_LEVELS 2.0
#define AMOUNT_MIN 0.8
#define AMOUNT_MAX 0.2

    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(-u_lightDirection);
    float amount = dot(normal, lightDirection);

    amount *= COLOUR_LEVELS;
    amount = round(amount);
    amount /= COLOUR_LEVELS;
    amount = (amount * AMOUNT_MAX) + AMOUNT_MIN;

    colour *= amount;


    vec2 xy = gl_FragCoord.xy;
    int x = int(mod(xy.x, MatrixSize));
    int y = int(mod(xy.y, MatrixSize));

    float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));

    if(alpha < 0.1) discard;

    FRAG_OUT = vec4(colour, 1.0);
    NORM_OUT = vec4(normal, 1.0);
    LIGHT_OUT = vec4(vec3(0.0), 1.0);
    POS_OUT = vec4(v_worldPosition, 1.0);
})";

static const inline std::string WeatherVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;

#include CAMERA_UBO

    uniform mat4 u_worldMatrix;
    uniform mat4 u_worldViewMatrix;

#include WIND_BUFFER
#include SCALE_BUFFER

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

#include CAMERA_UBO
#include WVP_UNIFORMS

uniform float u_size = 1.0;
uniform float u_progress = 0.0;

#include SCALE_BUFFER

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