/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef TL_BACKGROUND_SHADER_HPP_
#define TL_BACKGROUND_SHADER_HPP_

#include <string>

/*
Used with background elements to make them scroll with time.
*/

namespace Shaders
{
    namespace Hud
    {
        const static std::string TimerFragment = R"(
            uniform MED float u_time;

            varying MED vec2 v_texCoord0;

            const MED float start = 1.5708;
            const MED float end = 4.71239;
            const MED float pi = 3.1415926535;
            const MED float duration = 5.0;

            void main()
            {
                MED vec2 circlePos = vec2(0.5, 0.5);
                MED vec2 threshold = normalize(v_texCoord0 - circlePos);
    
                MED float T = mod(u_time, duration);
                MED float angle  = mix(start, end, cos(pi / duration  * T - pi) / 2.0 + 0.5);
                //MED float xReq = cos(angle);
                MED float yReq = sin(angle);

                MED vec2 sqr = circlePos - v_texCoord0;
                sqr *= sqr;
                float length = sqr.x + sqr.y;

                gl_FragColor = vec4(0.0, 1.0, 1.0, 0.0);
                if (length > 0.25 || length < 0.21)
                {
                    gl_FragColor.a = 0.0;
                }
                else if (threshold.y < yReq)
                {
                    gl_FragColor.a = 0.7;
                }
            })";
    }

    namespace FX
    {
        const static std::string Vertex = R"(
            attribute vec4 a_position;
            attribute MED vec2 a_texCoord0;

            uniform mat4 u_worldMatrix;
            uniform mat4 u_worldViewMatrix;               
            uniform mat4 u_projectionMatrix;
                
            varying MED vec2 v_texCoord0;

            void main()
            {
                mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                gl_Position = wvp * a_position;

                v_texCoord0 = a_texCoord0;
            })";

        static const std::string EMPFragment = R"(
            uniform MED float u_time;
            
            varying MED vec2 v_texCoord0;

            const MED float maxRadius = 50.0;
            const MED float thickness = 7.0;
            const MED vec2 centre = vec2(0.5);
            const MED float coordScale = 100.0; //need to scale the values up so that they are properly clamped in the correct range

            void main()
            {
                MED float time = fract(u_time);
                MED float outerRadius = maxRadius * time;
                MED float innerRadius = outerRadius - (thickness * time);
                MED vec2 direction = v_texCoord0 - centre;
                MED float currentRadius = sqrt(dot(direction, direction)) * coordScale;

                MED float strength = clamp(currentRadius - innerRadius, 0.0, 1.0) * clamp(outerRadius - currentRadius, 0.0, 1.0);
                strength *= (1.0 - (outerRadius / maxRadius)); //fades as outrad grows

                LOW vec4 empColour = vec4(0.63, 0.0, 1.0, 1.0);
                LOW vec4 clearColour = vec4(0.0, 0.0, 0.0, 0.0);

                gl_FragColor = mix(clearColour, empColour, strength);               
            })";
    }

    namespace Background
    {
        const static std::string Vertex = R"(
            attribute vec4 a_position;
            attribute MED vec2 a_texCoord0;

            uniform mat4 u_worldMatrix;
            uniform mat4 u_worldViewMatrix;               
            uniform mat4 u_projectionMatrix;
            uniform vec2 u_textureOffset;
                
            varying MED vec2 v_texCoord0;

            void main()
            {
                mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                gl_Position = wvp * a_position;

                v_texCoord0 = a_texCoord0 + u_textureOffset;
            })";

        const static std::string Fragment = R"(
            precision mediump float;
            uniform sampler2D u_diffuseMap;
            uniform MED float u_colourAngle;

            varying MED vec2 v_texCoord0;
                

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


            void main()
            {
                vec4 colour = texture2D(u_diffuseMap, v_texCoord0);
                vec3 hsv = rgb2hsv(colour.rgb);
                hsv.x += u_colourAngle / 360.0;
                hsv.x = mod(hsv.x, 1.0);
                gl_FragColor = vec4(hsv2rgb(hsv), colour.a);
            })";
    }
}

#endif //TL_BACKGROUND_SHADER_HPP_