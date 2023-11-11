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

inline const std::string UnlockVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec4 a_colour;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat3 u_normalMatrix;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_worldPosition;
    VARYING_OUT vec4 v_colour;

    VARYING_OUT vec2 v_texCoord;

    void main()
    {
        vec4 position = a_position;
        gl_Position = u_viewProjectionMatrix * u_worldMatrix * position;

        vec3 normal = a_normal;
        v_normal = u_normalMatrix * normal;

        v_worldPosition = u_worldMatrix * a_position;
        v_colour = a_colour;
    })";

inline const std::string UnlockFragment = R"(
    OUTPUT

#if defined(REFLECTION)
    uniform samplerCube u_reflectMap;
#endif

    uniform vec3 u_cameraWorldPosition;
    uniform vec3 u_lightDirection = vec3(0.1, -0.5, 0.4);

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_worldPosition;
    VARYING_IN vec4 v_colour;

    VARYING_IN vec2 v_texCoord;

    void main()
    {
        vec4 colour = v_colour;
        vec3 normal = normalize(v_normal);

        float amount = dot(normal, normalize(-u_lightDirection));

        amount *= 2.0;
        amount = round(amount);
        amount /= 2.0;
        amount = 0.8 + (amount * 0.2);
        colour.rgb *= amount;

#if defined (REFLECTION)
        vec3 viewDirection = normalize(u_cameraWorldPosition - v_worldPosition.xyz);
        colour.rgb += TEXTURE_CUBE(u_reflectMap, reflect(-viewDirection, normal)).rgb * 0.25;
#endif

        FRAG_OUT = colour;
    }
)";