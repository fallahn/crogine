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
        ATTRIBUTE vec4 a_colour; //radius, rotation offset and speed

        uniform mat4 u_projection; //TODO is this available in the UBO (and should particle system be using it?)
        uniform mat4 u_viewProjection;
        uniform vec4 u_clipPlane;
        uniform float u_viewportHeight;

        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT LOW float v_currentFrame;
        VARYING_OUT HIGH float v_depth;

        const float particleSize = 10.0;
//TODO include wind data
        void main()
        {
            v_colour = a_colour;

            v_currentFrame = 0.0;

            gl_Position = u_viewProjection * a_position;
            gl_PointSize = u_viewportHeight * u_projection[1][1] / gl_Position.w * particleSize;

            v_depth = gl_Position.z / gl_Position.w;

            gl_ClipDistance[0] = dot(a_position, u_clipPlane);
        }
)";

static inline const std::string SwarmFragment =
R"(
        uniform sampler2D u_texture;
        uniform float u_frameCount = 6.0;
        uniform vec2 u_textureSize;
        uniform vec2 u_cameraRange;

#if defined (SUNLIGHT)
        uniform vec4 u_lightColour;
#endif

        VARYING_IN LOW vec4 v_colour;
        VARYING_IN LOW float v_currentFrame;
        VARYING_IN HIGH float v_depth;
        OUTPUT

layout (location = 3) out vec4 LIGHT_OUT;


        void main()
        {
            float frameWidth = 1.0 / u_frameCount;
            vec2 coord = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
            coord.x *= frameWidth;
            coord.x += v_currentFrame * frameWidth;
            //vec2 centreOffset = vec2((v_currentFrame * frameWidth) + (frameWidth / 2.f), 0.5);

            //convert to texture space
            //coord *= u_textureSize;
            //centreOffset *= u_textureSize;

            //rotate
            //coord = v_rotation * (coord - centreOffset);
            //coord += centreOffset;

            //and back to UV space
            //coord /= u_textureSize;

            FRAG_OUT = vec4(1.0);// v_colour * TEXTURE(u_texture, coord);

        #if defined (SUNLIGHT)
            FRAG_OUT *= u_lightColour;
        #endif

        #if defined (BLEND_ADD)
            FRAG_OUT.rgb *= v_colour.a;
        #endif

        #if defined (BLEND_MULTIPLY)
            FRAG_OUT.rgb += (vec3(1.0) - FRAG_OUT.rgb) * (1.0 - FRAG_OUT.a);
        #endif


LIGHT_OUT = vec4(vec3(0.0), 1.0);

        }
)";