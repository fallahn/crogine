/*-----------------------------------------------------------------------

Matt Marchant 2022
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

static const std::string CloudVertex = 
R"(
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    ATTRIBUTE vec4 a_position;
    ATTRIBUTE MED vec2 a_texCoord0;

    VARYING_OUT LOW vec3 v_worldPos;
    VARYING_OUT MED vec2 v_texCoord;

    void main()
    {
        vec4 worldPos = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * worldPos;
        
        v_worldPos = worldPos.xyz;

        v_texCoord = a_texCoord0;
    }
)";

static const std::string CloudFragment =
R"(
    OUTPUT

    uniform sampler2D u_texture;
    uniform vec2 u_worldCentre = vec2(0.0);
    uniform float u_pixelScale = 1.0;

    VARYING_IN vec3 v_worldPos;
    VARYING_IN vec2 v_texCoord;

    const float MaxDist = 180.0;


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
    }


    void main()
    {
        FRAG_OUT = TEXTURE(u_texture, v_texCoord);

        float amount = 1.0 - smoothstep(0.7, 1.0, (length(v_worldPos.xz - u_worldCentre) / MaxDist));

        vec2 xy = gl_FragCoord.xy / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));
        float alpha = findClosest(x, y, amount);

        if(alpha < 0.1) discard;
    }
)";