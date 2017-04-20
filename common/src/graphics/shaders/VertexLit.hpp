/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_SHADER_VERTEX_HPP_
#define CRO_SHADER_VERTEX_HPP_

#include <string>

namespace cro
{
    namespace Shaders
    {
        namespace VertexLit
        {
            const static std::string Vertex = R"(
                attribute vec3 a_position;
                #if defined (VERTEX_COLOUR)
                attribute vec3 a_colour;
                #endif
                attribute vec3 a_normal;
                #if defined(BUMP)
                attribute vec3 a_tangent;
                attribute vec3 a_bitangent;
                #endif
                #if defined(TEXTURED)
                attribute vec2 a_texCoord0;
                #if defined (LIGHTMAPPED)
                attribute vec2 a_texCoord1;
                #endif
                #endif

                uniform mat4 u_worldMatrix;
                uniform mat4 u_worldViewMatrix;
                uniform mat3 u_normalMatrix;                
                uniform mat4 u_projectionMatrix;
                
                varying vec3 v_worldPosition;
                #if defined (VERTEX_COLOUR)
                varying vec3 v_colour;
                #endif
                varying vec3 v_normalVector;
                #if defined(TEXTURED)
                varying vec2 v_texCoord0;
                #if defined(LIGHTMAPPED)
                varying vec2 v_texCoord1;
                #endif
                #endif


                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    gl_Position = wvp * vec4(a_position, 1.0);

                    v_worldPosition = (u_worldMatrix * vec4(a_position, 1.0)).xyz;
                #if defined(VERTEX_COLOUR)
                    v_colour = a_colour;
                #endif
                    v_normalVector = u_normalMatrix * a_normal;
                #if defined(TEXTURED)
                    v_texCoord0 = a_texCoord0;
                #if defined(LIGHTMAPPED)
                    v_texCoord1 = a_texCoord1;
                #endif
                #endif
                })";

                const static std::string Fragment = R"(
                #if defined(TEXTURED)
                uniform sampler2D u_diffuseMap;
                #if defined(BUMP)
                uniform sampler2D u_normalMap;
                uniform sampler2D u_maskMap;
                #if defined(LIGHTMAPPED)
                uniform sampler2D u_lightMap;
                #endif
                #endif
                #endif

                uniform vec3 u_cameraWorldPosition;
                #if defined(COLOURED)
                uniform vec4 u_colour;
                #endif

                varying vec3 v_worldPosition;
                #if defined(VERTEX_COLOUR)
                varying vec3 v_colour;
                #endif
                varying vec3 v_normalVector;
                #if defined(TEXTURED)
                varying vec2 v_texCoord0;
                #if defined(LIGHTMAPPED)
                varying vec2 v_texCoord1;
                #endif
                #endif
                
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
                #if defined (TEXTURED)
                    vec4 diffuse = texture2D(u_diffuseMap, v_texCoord0);
                #elif defined(COLOURED)
                    vec4 diffuse = u_colour;
                #elif defined(VERTEX_COLOURED)
                    vec4 diffuse = v_colour;
                #else
                    vec4 diffuse = vec4(1.0);
                #endif
                    diffuseColour = diffuse.rgb;
                    vec3 blendedColour = diffuse.rgb * 0.2; //ambience
                    eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

                    blendedColour += calcLighting(normal, normalize(-lightDir), vec3(0.18), vec3(1.0), 1.0);
                    gl_FragColor.rgb = blendedColour;

                    //gl_FragColor = vec4(v_texCoord0.x, v_texCoord0.y, 1.0, 1.0);
                    //gl_FragColor.rgb *= v_normalVector;
                    //gl_FragColor *= texture2D(u_diffuseMap, v_texCoord0);
                    //gl_FragColor *= texture2D(u_normalMap, v_texCoord0);
                })";
        }
    }
}

#endif //CRO_SHADER_VERTEX_HPP_