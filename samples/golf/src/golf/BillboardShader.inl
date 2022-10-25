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

//because the billboards are based on the view direction when casting
//shadows we need to use the game camera to set up vert positions.
static const std::string BillboardVertexShader = R"(
    ATTRIBUTE vec4 a_position; //relative to root position (below)
    ATTRIBUTE vec3 a_normal; //actually contains root position of billboard
    ATTRIBUTE vec4 a_colour;

    ATTRIBUTE MED vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform sampler2D u_noiseTexture;

#if defined(SHADOW_MAPPING)
    uniform mat4 u_cameraViewMatrix;
    uniform mat4 u_projectionMatrix;
#endif


    uniform vec4 u_clipPlane;
    uniform vec3 u_cameraWorldPosition;

    #include WIND_BUFFER

    layout (std140) uniform ScaledResolution
    {
        vec2 u_scaledResolution;
        float u_nearFadeDistance;
    };

    VARYING_OUT LOW vec4 v_colour;
    VARYING_OUT MED vec2 v_texCoord0;

    VARYING_OUT float v_ditherAmount;

    struct WindResult
    {
        vec2 highFreq;
        vec2 lowFreq;
        float strength;
    };

    const float hFreq = 0.05;
    const float hMagnitude = 0.08;
    const float lFreq = 0.008;
    const float lMagnitude = 0.2;
    const float dirMagnitude = 0.3;
    WindResult getWindData(vec2 coord, vec2 coord2) //coord2 is root pos
    {
        WindResult retVal = WindResult(vec2(0.0), vec2(0.0), 0.0);
        vec2 uv = coord;
        uv.x += u_windData.w * hFreq;
        retVal.highFreq.x = TEXTURE(u_noiseTexture, uv).r;

        uv = coord;
        uv.y += u_windData.w * hFreq;
        retVal.highFreq.y = TEXTURE(u_noiseTexture, uv).r;

        uv = coord2;
        uv.x -= u_windData.w * lFreq;
        retVal.lowFreq.x = TEXTURE(u_noiseTexture, uv).r;

        uv = coord2;
        uv.y -= u_windData.w * lFreq;
        retVal.lowFreq.y = TEXTURE(u_noiseTexture, uv).r;


        retVal.highFreq *= 2.0;
        retVal.highFreq -= 1.0;
        retVal.highFreq *= u_windData.y;
        retVal.highFreq *= hMagnitude;

        retVal.lowFreq *= 2.0;
        retVal.lowFreq -= 1.0;
        retVal.lowFreq *= (0.6 + (0.4 * u_windData.y));
        retVal.lowFreq *= lMagnitude;

        retVal.strength = u_windData.y;
        retVal.strength *= dirMagnitude;

        return retVal;
    }


    void main()
    {
        //red low freq, green high freq, blue direction amount

        WindResult windResult = getWindData(a_normal.xz + a_position.xz, a_normal.xz); //normal is root billboard position - a_position is relative
        vec3 vertexStrength = step(0.1, a_position.y) * (vec3(1.0) - a_colour.rgb);
        //multiply high and low frequency by vertex colours
        windResult.lowFreq *= vertexStrength.r;
        windResult.highFreq *= vertexStrength.g;

        //apply high frequency and low frequency in local space
        vec3 relPos = a_position.xyz;
        relPos.x += windResult.lowFreq.x + windResult.highFreq.x;
        relPos.z += windResult.lowFreq.y + windResult.highFreq.y;

        //multiply wind direction by wind strength
        vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * vertexStrength.b;
        //wind dir is added in world space (below)



        vec3 position = (u_worldMatrix * vec4(a_normal, 1.0)).xyz;

#if defined (SHADOW_MAPPING)
        mat4 viewMatrix = u_cameraViewMatrix;
        mat4 viewProj = u_projectionMatrix * u_viewMatrix;
#else
        mat4 viewMatrix = u_viewMatrix;
        mat4 viewProj = u_viewProjectionMatrix;
#endif
        vec3 camRight = vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
        vec3 camUp = vec3(0.0, 1.0, 0.0);
        position = position + camRight * relPos.x
                            + camUp * relPos.y
                            + cross(camRight, camUp) * relPos.z;


        //---generic wind added to tall billboards---//
        const float xFreq = 0.03;// 0.6;
        const float yFreq = 0.015;// 0.8;

        const float scale = 0.2;
        const float minHeight = 3.0;
        const float maxHeight = 12.0;

        //default animate above 3 metres
        float height = max(0.0, a_position.y - minHeight);

        float strength = u_windData.y;
        float totalScale = scale * (height / maxHeight) * strength;


        vec2 uv = a_normal.xz;
        uv.x += u_windData.w * xFreq;
        float windX = TEXTURE(u_noiseTexture, uv).r;
        windX *= 2.0;
        windX -= 1.0;

        uv = a_normal.xz;
        uv.y += u_windData.w * yFreq;
        float windZ = TEXTURE(u_noiseTexture, uv).r;
        windZ *= 2.0;
        windZ -= 1.0;


        position.x += windX * totalScale;
        position.z += windZ * totalScale;
        position.xz += (u_windData.xz * strength * 2.0) * totalScale;
        //------------------------------//

        //wind from billboard vertex colour (above)
        position += windDir;

        v_colour.rgb = vec3(1.0);


        //snap vert pos to nearest fragment for retro wobble
        //hmm not so good when coupled with wind (above)
        vec4 vertPos = viewProj * vec4(position, 1.0);
        /*vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;*/
        gl_Position = vertPos;

        v_texCoord0 = a_texCoord0;

#if !defined(SHADOW_MAPPING)

        float fadeDistance = u_nearFadeDistance * 5.0;
        const float farFadeDistance = 300.f;
        float distance = length(position - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 5.0);
        v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / fadeDistance, 0.0, 1.0);


        v_colour.rgb *= (((1.0 - pow(clamp(distance / farFadeDistance, 0.0, 1.0), 5.0)) * 0.8) + 0.2);
#endif
        gl_ClipDistance[0] = dot(u_worldMatrix * vec4(position, 1.0), u_clipPlane);

    })";


static const std::string BillboardFragmentShader = R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;

    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };

    VARYING_IN LOW vec4 v_colour;
    VARYING_IN MED vec2 v_texCoord0;

    VARYING_IN float v_ditherAmount;
    VARYING_IN vec3 v_normalVector;
    VARYING_IN HIGH vec3 v_worldPosition;

    //function based on example by martinsh.blogspot.com
    const int MatrixSize = 8;
    float findClosest(int x, int y, float c0)
    {
        /* 8x8 Bayer ordered dithering */
        /* pattern. Each input pixel */
        /* is scaled to the 0..63 range */
        /* before looking in this table */
        /* to determine the action. */

        const int dither[64] = int[64](
            0,  32, 8,  40, 2,  34, 10, 42, 
            48, 16, 56, 24, 50, 18, 58, 26, 
            12, 44, 4,  36, 14, 46, 6,  38, 
            60, 28, 52, 20, 62, 30, 54, 22, 
            3,  35, 11, 43, 1,  33, 9,  41, 
            51, 19, 59, 27, 49, 17, 57, 25,
            15, 47, 7,  39, 13, 45, 5,  37,
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
    }


    void main()
    {
        FRAG_OUT = v_colour;
        FRAG_OUT *= TEXTURE(u_diffuseMap, v_texCoord0);

        vec2 xy = gl_FragCoord.xy / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));
        float alpha = findClosest(x, y, smoothstep(0.1, 0.999, v_ditherAmount));
        FRAG_OUT.a *= alpha;

        if(FRAG_OUT.a < 0.2) discard;
    })";