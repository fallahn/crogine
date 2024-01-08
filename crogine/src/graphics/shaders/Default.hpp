/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
http://trederia.blogspot.com

crogine - Zlib license.

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

namespace cro::Shaders::Default
{
    inline const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE LOW vec4 a_colour;
        ATTRIBUTE vec3 a_normal;
        ATTRIBUTE vec3 a_tangent;
        ATTRIBUTE vec3 a_bitangent;
        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE MED vec2 a_texCoord1;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_worldViewMatrix;
        uniform mat3 u_normalMatrix;                
        uniform mat4 u_projectionMatrix;
                
        VARYING_OUT vec3 v_worldPosition;
        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT vec3 v_normalVector;
        VARYING_OUT MED vec2 v_texCoord0;
        VARYING_OUT MED vec2 v_texCoord1;


        void main()
        {
            mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
            gl_Position = wvp * a_position;

            v_worldPosition = (u_worldMatrix * a_position).xyz;

            v_colour = a_colour;
            v_normalVector = u_normalMatrix * a_normal;

            v_texCoord0 = a_texCoord0;
            v_texCoord1 = a_texCoord1;

        })";

    inline const std::string Fragment = R"(

        uniform sampler2D u_diffuseMap;
        uniform sampler2D u_normalMap;
        uniform sampler2D u_maskMap;
        uniform sampler2D u_lightMap;

        uniform vec3 u_cameraWorldPosition;

        VARYING_IN vec3 v_worldPosition;
        VARYING_IN vec4 v_colour;
        VARYING_IN vec3 v_normalVector;
        VARYING_IN vec2 v_texCoord0;
        VARYING_IN vec2 v_texCoord1;

        OUTPUT                

        vec3 lightDir = vec3(0.1, -0.8, -0.2);

        vec3 diffuseColour;
        vec3 eyeDirection;
        vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
        {
            float diffuseAmount = max(dot(normal, lightDirection), 0.0);
            //diffuseAmount = pow((diffuseAmount * 0.5) + 5.0, 2.0);
            vec3 mixedColour = diffuseColour * lightDiffuse * diffuseAmount * falloff;

            vec3 halfVec = normalize(eyeDirection + lightDirection);
            float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
            vec3 specularColour = lightSpecular * vec3(pow(specularAngle, 255.0)) * falloff;

            return mixedColour + specularColour;
        }

        void main()
        {
            vec3 normal = normalize(v_normalVector);
            vec4 diffuse = TEXTURE(u_diffuseMap, v_texCoord0);
            diffuseColour = diffuse.rgb;
            vec3 blendedColour = diffuse.rgb * 0.2; //ambience
            eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

            blendedColour += calcLighting(normal, normalize(-lightDir), vec3(0.18), vec3(1.0), 1.0);
            FRAG_OUT.rgb = blendedColour;

            //FRAG_OUT = vec4(v_texCoord0.x, v_texCoord0.y, 1.0, 1.0);
            //FRAG_OUT.rgb *= v_normalVector;
            //FRAG_OUT *= TEXTURE(u_diffuseMap, v_texCoord0);
            //FRAG_OUT *= TEXTURE(u_normalMap, v_texCoord0);
        })";
}
