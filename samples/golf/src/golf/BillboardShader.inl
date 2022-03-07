/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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


static const std::string BillboardVertexShader = R"(
    ATTRIBUTE vec4 a_position; //relative to root position (below)
    ATTRIBUTE vec3 a_normal; //actually contains root position of billboard
    ATTRIBUTE vec4 a_colour;

    ATTRIBUTE MED vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform vec4 u_clipPlane;
    uniform vec3 u_cameraWorldPosition;
    uniform vec2 u_scaledResolution;

    uniform float u_time = 0.0;
    uniform vec3 u_windDir = vec3(0.5);

    VARYING_OUT LOW vec4 v_colour;
    VARYING_OUT MED vec2 v_texCoord0;

    VARYING_OUT float v_ditherAmount;

    void main()
    {
        vec3 position = (u_worldMatrix * vec4(a_normal, 1.0)).xyz;

        vec3 camRight = vec3(u_viewMatrix[0][0], u_viewMatrix[1][0], u_viewMatrix[2][0]);
        vec3 camUp = vec3(0.0, 1.0, 0.0);
        position = position + camRight * a_position.x
                            + camUp * a_position.y;


        const float xFreq = 0.6;
        const float yFreq = 0.8;
        const float scale = 0.2;
        const float minHeight = 3.0;
        const float maxHeight = 12.0;

        //only animate above 3 metres
        float height = max(0.0, position.y - minHeight);

        float strength = u_windDir.y;
        float totalScale = scale * (height / maxHeight) * strength;

        position.x += sin((u_time * (xFreq)) + a_normal.x) * totalScale;
        position.z += sin((u_time * (yFreq)) + a_normal.z) * totalScale;
        position.xz += (u_windDir.xz * strength * 2.0) * totalScale;




        //snap vert pos to nearest fragment for retro wobble
        //hmm not so good when coupled with wind (above)
        vec4 vertPos = u_viewProjectionMatrix * vec4(position, 1.0);
        /*vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;*/
        gl_Position = vertPos;

        v_colour = a_colour;

        const float minDistance = 2.0;
        const float nearFadeDistance = 10.0; //TODO make this a uniform
        const float farFadeDistance = 300.f;
        float distance = length(position - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((distance - minDistance) / nearFadeDistance, 0.0, 1.0), 5.0);
        v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / nearFadeDistance, 0.0, 1.0);

        v_colour.rgb *= (((1.0 - pow(clamp(distance / farFadeDistance, 0.0, 1.0), 5.0)) * 0.8) + 0.2);

        v_texCoord0 = a_texCoord0;

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