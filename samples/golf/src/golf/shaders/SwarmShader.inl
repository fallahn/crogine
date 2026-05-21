/*-----------------------------------------------------------------------

Matt Marchant 2025
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

static inline const std::string SwarmVertex =
R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE vec4 a_colour; //radius, rotation offset, speed and animation offset

        uniform sampler2D u_noiseTexture;
#include CAMERA_UBO
        uniform mat4 u_worldMatrix;
#include RESOLUTION_BUFFER

//time is in u_windData.w
#include WIND_BUFFER

        VARYING_OUT float v_currentFrame;

        const float particleSize = 64.0;
        const float PI = 3.141;
#if !defined(AREA_SIZE)
#define AREA_SIZE 2.5
#endif

#if !defined(FRAME_COUNT)
#define FRAME_COUNT 4.0
#endif

#if !defined(FRAME_RATE)
#define FRAME_RATE (1.0/18.0)
#endif


        void main()
        {
            v_currentFrame = mod(round(u_windData.w/FRAME_RATE) + round(a_colour.a * FRAME_COUNT), FRAME_COUNT);

            vec4 worldPos = u_worldMatrix * a_position;

            //offset by radius
            float time = (u_windData.w) + (a_colour.g * PI);
            float speed = a_colour.b * 2.0 - 1.0;
            vec2 offset = vec2(sin(time * speed) * a_colour.r,
                                cos(time * speed) * a_colour.r);
            worldPos.xz += offset;

            vec2 uv = (a_position.xz + offset) / AREA_SIZE;
            worldPos.y += (TEXTURE(u_noiseTexture, uv).b * 2.0 - 1.0) * a_colour.b * 0.5;


            gl_Position = u_viewProjectionMatrix * worldPos;
             //this would also be multiplied by normlised viewport height if anything other than 1
            float size = particleSize * (u_bufferResolution.y / 480.0); //TODO account for pixel scale too
            gl_PointSize = u_projectionMatrix[1][1] / gl_Position.w * size;
//TODO also u_nearFadeDistance?
            gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
        }
)";

static inline const std::string SwarmFragment =
R"(
        uniform sampler2D u_texture;
#if defined(ILLUM)
        uniform sampler2D u_mask;
        //light mask is stored in norm.a
        layout (location = 2) out vec4 NORM_OUT;
#endif
        uniform vec4 u_lightColour;
#include LIGHT_COLOUR

        VARYING_IN float v_currentFrame;

        OUTPUT

        layout (location = 3) out vec4 LIGHT_OUT;

#if !defined(FRAME_COUNT)
#define FRAME_COUNT 4.0
#endif

        void main()
        {
            float frameWidth = 1.0 / FRAME_COUNT;
            vec2 coord = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
            coord.x *= frameWidth;
            coord.x += v_currentFrame * frameWidth;

            FRAG_OUT = TEXTURE(u_texture, coord);
            if (FRAG_OUT.a < 0.5) discard;

#if !defined(ILLUM)
            LIGHT_OUT = vec4(vec3(0.0), 1.0);
            FRAG_OUT *= getLightColour();
#else
            float mask = TEXTURE(u_mask, coord).g;
            LIGHT_OUT = vec4(FRAG_OUT.rgb * mask, 1.0);
            NORM_OUT.a = mask;
#endif

//TODO also fog colour?

        #if defined (BLEND_ADD)
            FRAG_OUT.rgb *= v_colour.a;
        #endif

        #if defined (BLEND_MULTIPLY)
            FRAG_OUT.rgb += (vec3(1.0) - FRAG_OUT.rgb) * (1.0 - FRAG_OUT.a);
        #endif
})";