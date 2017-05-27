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
                attribute vec4 a_position;
                #if defined (VERTEX_COLOUR)
                attribute LOW vec4 a_colour;
                #endif
                attribute vec3 a_normal;
                #if defined(BUMP)
                attribute vec3 a_tangent;
                attribute vec3 a_bitangent;
                #endif
                #if defined(TEXTURED)
                attribute MED vec2 a_texCoord0;
                #if defined (LIGHTMAPPED)
                attribute MED vec2 a_texCoord1;
                #endif
                #endif

                #if defined(SKINNED)
                attribute vec4 a_boneIndices;
                attribute vec4 a_boneWeights;
                uniform mat4 u_boneMatrices[MAX_BONES];
                #endif

                uniform mat4 u_worldMatrix;
                uniform mat4 u_worldViewMatrix;
                uniform mat3 u_normalMatrix;                
                uniform mat4 u_projectionMatrix;
                #if defined (SUBRECTS)
                uniform MED vec4 u_subrect;
                #endif
                
                varying vec3 v_worldPosition;
                #if defined (VERTEX_COLOUR)
                varying LOW vec4 v_colour;
                #endif
                #if defined(BUMP)
                varying vec3 v_tbn[3];
                #else
                varying vec3 v_normalVector;
                #endif
                #if defined(TEXTURED)
                varying MED vec2 v_texCoord0;
                #if defined(LIGHTMAPPED)
                varying MED vec2 v_texCoord1;
                #endif
                #endif


                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    vec4 position = a_position;

                #if defined(SKINNED)
                	//int idx = 0;//int(a_boneIndices.x);
                    mat4 skinMatrix = u_boneMatrices[int(a_boneIndices.x)] * a_boneWeights.x;
                	skinMatrix += u_boneMatrices[int(a_boneIndices.y)] * a_boneWeights.y;
                	skinMatrix += u_boneMatrices[int(a_boneIndices.z)] * a_boneWeights.z;
                	skinMatrix += u_boneMatrices[int(a_boneIndices.w)] * a_boneWeights.w;
                	position = skinMatrix * position;
                #endif

                    gl_Position = wvp * position;

                    v_worldPosition = (u_worldMatrix * a_position).xyz;
                #if defined(VERTEX_COLOUR)
                    v_colour = a_colour;
                #endif

                vec3 normal = a_normal;

                #if defined(SKINNED)
                    normal = (skinMatrix * vec4(normal, 0.0)).xyz;
                #endif

                #if defined (BUMP)
                    vec4 tangent = vec4(a_tangent, 0.0);
                    vec4 bitangent = vec4(a_bitangent, 0.0);
                #if defined (SKINNED)
                    tangent = skinMatrix * tangent;
                    bitangent = skinMatrix * bitangent;
                #endif
                    v_tbn[0] = normalize(u_worldMatrix * tangent).xyz;
                    v_tbn[1] = normalize(u_worldMatrix * bitangent).xyz;
                    v_tbn[2] = normalize(u_worldMatrix * vec4(normal, 0.0)).xyz;
                #else
                    v_normalVector = u_normalMatrix * normal;
                #endif

                #if defined(TEXTURED)
                #if defined (SUBRECTS)
                    v_texCoord0 = u_subrect.xy + (a_texCoord0 * u_subrect.zw);
                #else
                    v_texCoord0 = a_texCoord0;                    
                #endif
                #if defined(LIGHTMAPPED)
                    v_texCoord1 = a_texCoord1;
                #endif
                #endif
                })";

            const static std::string Fragment = R"(
                #if defined(TEXTURED)
                uniform sampler2D u_diffuseMap;
                uniform sampler2D u_maskMap;
                #if defined(BUMP)
                uniform sampler2D u_normalMap;
                #if defined(LIGHTMAPPED)
                uniform sampler2D u_lightMap;
                #endif
                #endif
                #endif

                uniform HIGH vec3 u_cameraWorldPosition;
                #if defined(COLOURED)
                uniform LOW vec4 u_colour;
                #endif

                varying HIGH vec3 v_worldPosition;
                #if defined(VERTEX_COLOUR)
                varying LOW vec4 v_colour;
                #endif
                #if defined (BUMP)
                varying HIGH vec3 v_tbn[3];
                #else
                varying HIGH vec3 v_normalVector;
                #endif
                #if defined(TEXTURED)
                varying MED vec2 v_texCoord0;
                #if defined(LIGHTMAPPED)
                varying MED vec2 v_texCoord1;
                #endif
                #endif
                
                HIGH vec3 lightDir = vec3(0.1, -0.8, -0.2);

                LOW vec3 diffuseColour;
                HIGH vec3 eyeDirection;
                LOW vec3 mask;
                vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
                {
                    MED float diffuseAmount = max(dot(normal, lightDirection), 0.0);
                    //diffuseAmount = pow((diffuseAmount * 0.5) + 5.0, 2.0);
                    MED vec3 mixedColour = diffuseColour * lightDiffuse * diffuseAmount * falloff;

                    MED vec3 halfVec = normalize(eyeDirection + lightDirection);
                    MED float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
                    LOW vec3 specularColour = lightSpecular * vec3(pow(specularAngle, ((254.0 * mask.r) + 1.0))) * falloff;

                    return mixedColour + (specularColour * mask.g);
                }

                void main()
                {
                #if defined (BUMP)
                    MED vec3 texNormal = texture2D(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
                    MED vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
                #else
                    MED vec3 normal = normalize(v_normalVector);
                #endif
                #if defined (TEXTURED)
                    LOW vec4 diffuse = texture2D(u_diffuseMap, v_texCoord0);
                    mask = texture2D(u_maskMap, v_texCoord0).rgb;
                #elif defined(COLOURED)
                    LOW vec4 diffuse = u_colour;
                #elif defined(VERTEX_COLOURED)
                    LOW vec4 diffuse = v_colour;
                #else
                    LOW vec4 diffuse = vec4(1.0);
                #endif
                    diffuseColour = diffuse.rgb;
                    LOW vec3 blendedColour = diffuse.rgb * 0.2; //ambience
                    eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

                    blendedColour += calcLighting(normal, normalize(-lightDir), vec3(0.48), vec3(1.0), 1.0);
                #if defined(TEXTURED)
                    gl_FragColor.rgb = mix(blendedColour, diffuseColour, mask.b);
                #else     
                    gl_FragColor.rgb = blendedColour;
                #endif
                    gl_FragColor.a = diffuse.a;
                })";
        }
    }
}

#endif //CRO_SHADER_VERTEX_HPP_