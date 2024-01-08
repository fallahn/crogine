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

#include <string>
#include <unordered_map>

static inline const std::string WindBuffer = R"(
    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };)";

static inline const std::string ResolutionBuffer = R"(
    layout (std140) uniform ScaledResolution
    {
        vec2 u_scaledResolution;
        vec2 u_bufferResolution;
        float u_nearFadeDistance;
    };)";

static inline const std::string ScaleBuffer = R"(
    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };)";

/*
function based on example by martinsh.blogspot.com
8x8 Bayer ordered dithering
pattern. Each input pixel
is scaled to the 0..63 range
before looking in this table
to determine the action. 
*/
static inline const std::string BayerMatrix = R"(
    const int MatrixSize = 8;
    float findClosest(int x, int y, float c0)
    {
        const int dither[64] = int[64](
         0, 32, 8, 40, 2, 34, 10, 42, 
        48, 16, 56, 24, 50, 18, 58, 26, 
        12, 44, 4, 36, 14, 46, 6, 38, 
        60, 28, 52, 20, 62, 30, 54, 22, 
         3, 35, 11, 43, 1, 33, 9, 41, 
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47, 7, 39, 13, 45, 5, 37,
        63, 31, 55, 23, 61, 29, 53, 21 );

        float limit = 0.0;
        if (x < MatrixSize)
        {
            limit = (dither[y * MatrixSize + x] + 1) / 64.0;
        }

        if (c0 < limit)
        {
            return 0.0;
        }
        return 1.0;
    })";

//https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
static inline const std::string Random = R"(
    float rand(float n)
    {
        return fract(sin(n) * 43758.5453123);
    }

#define noise rand
    float rand(vec2 position)
    {
        return fract(sin(dot(position, vec2(12.9898, 4.1414))) * 43758.5453);
    }



    //float noise2(vec2 n)
    //{
	   // const vec2 d = vec2(0.0, 1.0);
    //    vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
	   // return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
    //}

)";

//https://gist.github.com/yiwenl/745bfea7f04c456e0101
//or possibly https://gamedev.stackexchange.com/a/59808 ??
static inline const std::string HSV = R"(
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
        vec4 K = vec4(1.0, 0.666667, 0.333333, 3.0);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }
)";

//requires u_noiseTexture containing 2D noise
//and u_windData from WIND_BUFFER
//dirX, strength, dirZ, elapsed time - xyzw
//localPos is local position of vertex
//world pos is the root pos of the current mesh
static inline const std::string WindCalc = R"(
    struct WindResult
    {
        vec2 highFreq;
        vec2 lowFreq;
        float strength;
    };

    const float hFreq = 0.035;
    const float hMagnitude = 0.08;
    const float lFreq = 0.008;
    const float lMagnitude = 0.2;
    const float dirMagnitude = 0.3;
    WindResult getWindData(vec2 localPos, vec2 worldPos)
    {
        WindResult retVal = WindResult(vec2(0.0), vec2(0.0), 0.0);

        //vec2 uv = localPos;
        //uv.x = (u_windData.w * hFreq) + uv.x;
        //retVal.highFreq.x = TEXTURE(u_noiseTexture, uv).b;

        //uv = localPos;
        //uv.y = (u_windData.w * hFreq) + uv.y;
        //retVal.highFreq.y = TEXTURE(u_noiseTexture, uv).b;

        //uv = worldPos;
        //uv.x -= u_windData.w * lFreq;
        //retVal.lowFreq.x = TEXTURE(u_noiseTexture, uv).b;

        //uv = worldPos;
        //uv.y -= u_windData.w * lFreq;
        //retVal.lowFreq.y = TEXTURE(u_noiseTexture, uv).b;

        vec2 uv = vec2(u_windData.w * lFreq) + localPos;
        vec3 freqSample = TEXTURE(u_noiseTexture, uv).rgb;
        retVal.highFreq = freqSample.rg;

        uv = worldPos;
        uv -= u_windData.w * lFreq;
        freqSample = TEXTURE(u_noiseTexture, uv).rgb;
        retVal.lowFreq = freqSample.bb;


        retVal.highFreq *= 2.0;
        retVal.highFreq -= 1.0;
        retVal.highFreq *= u_windData.y;

        retVal.highFreq *= (hMagnitude * u_windData.y) + hMagnitude;

        retVal.lowFreq *= 2.0;
        retVal.lowFreq -= 1.0;
        retVal.lowFreq *= ((0.4 * u_windData.y)+ 0.6);

        retVal.lowFreq *= (lMagnitude * u_windData.y) + lMagnitude;

        retVal.strength = u_windData.y;
        retVal.strength *= dirMagnitude;

        return retVal;
    }
)";

/*
Decodes vector from VATs image data
*/
static inline const std::string VATVector = R"(
    #if defined (ARRAY_MAPPING)
        vec3 decodeVector(sampler2DArray source, vec3 coord)
    #else
        vec3 decodeVector(sampler2D source, vec2 coord)
    #endif
        {
            vec3 vec = texture(source, coord).rgb;
            vec *= 2.0;
            vec -= 1.0;

            return vec;
        }
)";

/*
Calculates a reduced-effect colour from the current 'sunlight' colour
*/
static inline const std::string LightColour = R"(
    vec4 getLightColour()
    {
        return vec4(clamp(u_lightColour * 1.55, 0.0, 1.0));
    }
)";

static inline const std::string OutputLocation = R"(
#if defined(USE_MRT)
    layout (location = 0) out vec4 FRAG_OUT;
    layout (location = 1) out vec4 POS_OUT;
    layout (location = 2) out vec4 NORM_OUT;
    layout (location = 3) out vec4 LIGHT_OUT;
    
#else

    OUTPUT
#endif

)";

static inline const std::string FogColour = R"(
const float ZNear = 0.1;
#if !defined(ZFAR)
#define ZFAR 320.0
#endif
const float ZFar = ZFAR;

#if !defined(DESAT)
#define DESAT 0.03
#endif

uniform float u_density = 0.0;
uniform float u_fogStart = 10.0;
uniform float u_fogEnd = ZFAR / 2.0;

const vec4 FogColour = vec4(0.91,0.92,0.923,1.0);

float fogAmount(float distance)
{
    //linear
    return clamp(smoothstep(u_fogStart, u_fogEnd, distance * ZFar) * u_density, 0.0, 1.0);

    //exp
    //distance = smoothstep(u_fogStart, u_fogEnd, distance * ZFar) * ZFar;
    //float density = 0.1 / u_density;
    //return 1.0 - clamp(exp2(-density * density * distance * distance), 0.1, 1.0);
}

float getDistance(float ds)
{
    //although this is "correct" it actually looks wrong.
    //float d = (2.0 * ZNear * ZFar) / (ZFar + ZNear - ds * (ZFar - ZNear));

    return (2.0 * ZNear) / (ZFar + ZNear - ds * (ZFar - ZNear));
}

vec3 dim(vec3 c)
{
    float desatAmount = clamp(u_density, 0.0, 1.0);
    vec3 desat = vec3(dot(c, vec3(0.299, 0.587, 0.114)));
    vec3 dimmed = c * (1.0 - (0.3 * desatAmount));
    return mix(dimmed, desat, desatAmount * DESAT);
})";

static inline const std::unordered_map<std::string, const char*> IncludeMappings =
{
    std::make_pair("WIND_BUFFER", WindBuffer.c_str()),
    std::make_pair("RESOLUTION_BUFFER", ResolutionBuffer.c_str()),
    std::make_pair("SCALE_BUFFER", ScaleBuffer.c_str()),
    std::make_pair("BAYER_MATRIX", BayerMatrix.c_str()),
    std::make_pair("RANDOM", Random.c_str()),
    std::make_pair("HSV", HSV.c_str()),
    std::make_pair("WIND_CALC", WindCalc.c_str()),
    std::make_pair("VAT_VEC", VATVector.c_str()),
    std::make_pair("LIGHT_COLOUR", LightColour.c_str()),
    std::make_pair("OUTPUT_LOCATION", OutputLocation.c_str()),
    std::make_pair("FOG_COLOUR", FogColour.c_str()),
};