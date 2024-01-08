/*-----------------------------------------------------------------------

Matt Marchant 2023
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

inline const std::string PointLightFrag =
R"(
        OUTPUT

        uniform sampler2D u_noiseTexture;
        uniform vec4 u_colour;
        uniform vec3 u_cameraWorldPosition;

#include WIND_BUFFER

        VARYING_IN vec3 v_normalVector;
        VARYING_IN vec3 v_worldPosition;

        void main()
        {
            vec3 normal = normalize(v_normalVector);
            vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

            float rim = abs(dot(normal, eyeDirection));
            rim = smoothstep(0.02, 0.95, rim);

            float n = texture(u_noiseTexture, vec2(u_windData.w * 0.2, 0.0)).r;
            rim *= (n * 0.5) + 0.5;

            vec3 colour = u_colour.rgb;
            n = texture(u_noiseTexture, vec2(u_windData.w * 0.2, 0.5)).r;
            n = (n * 0.3) + 0.7;
            colour.g *= n;
            colour.b *= n * 0.7;

            FRAG_OUT = vec4(colour * rim, 1.0);
        })";