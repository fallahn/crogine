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

namespace cro::Shaders::Deferred
{
    inline const std::string GBufferVertex = R"(
        ATTRIBUTE vec4 a_position;
    #if defined (VERTEX_COLOUR)
        ATTRIBUTE LOW vec4 a_colour;
    #endif
        ATTRIBUTE vec3 a_normal;
    #if defined(BUMP)
        ATTRIBUTE vec3 a_tangent;
        ATTRIBUTE vec3 a_bitangent;
    #endif
    #if defined(TEXTURED)
        ATTRIBUTE vec2 a_texCoord0;
    #endif

    #if defined(SKINNED)
        ATTRIBUTE vec4 a_boneIndices;
        ATTRIBUTE vec4 a_boneWeights;
        uniform mat4 u_boneMatrices[MAX_BONES];
    #endif

        uniform mat4 u_worldMatrix;
        uniform mat4 u_worldViewMatrix;
        uniform mat3 u_normalMatrix;
        uniform mat4 u_projectionMatrix;

        uniform vec4 u_clipPlane;
        
    #if defined(BUMP)
        VARYING_OUT vec3 v_tbn[3];
    #else
        VARYING_OUT vec3 v_normal;
    #endif

    #if defined(TEXTURED)
        VARYING_OUT vec2 v_texCoord;
    #endif
    #if defined (VERTEX_COLOUR)
        VARYING_OUT vec4 v_colour;
    #endif
        VARYING_OUT vec4 v_fragPosition;

        void main()
        {
            vec4 position = a_position;

        #if defined(SKINNED)
            mat4 skinMatrix = a_boneWeights.x * u_boneMatrices[int(a_boneIndices.x)];
            skinMatrix += a_boneWeights.y * u_boneMatrices[int(a_boneIndices.y)];
            skinMatrix += a_boneWeights.z * u_boneMatrices[int(a_boneIndices.z)];
            skinMatrix += a_boneWeights.w * u_boneMatrices[int(a_boneIndices.w)];
            position = skinMatrix * position;
        #endif

            v_fragPosition = u_worldViewMatrix * position;
            gl_Position = u_projectionMatrix * v_fragPosition; //we need this to render the depth buffer

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
            v_tbn[0] = normalize(u_normalMatrix * tangent.xyz);
            v_tbn[1] = normalize(u_normalMatrix * bitangent.xyz);
            v_tbn[2] = normalize(u_normalMatrix * normal);
        #else
            v_normal = u_normalMatrix * normal;
        #endif

        #if defined(TEXTURED)
            v_texCoord = a_texCoord0;
        #endif

        #if defined (MOBILE)

        #else
            gl_ClipDistance[0] = dot(u_worldMatrix * position, u_clipPlane);
        #endif
        #if defined(VERTEX_COLOUR)
            v_colour = a_colour;
        #endif
        })";

    inline const std::string GBufferFragment = R"(
    #if defined(DIFFUSE_MAP)
        uniform sampler2D u_diffuseMap;
    #endif
    #if defined (COLOURED)
        uniform vec4 u_colour;
    #endif
    #if defined(ALPHA_CLIP)
        uniform float u_alphaClip;
    #endif

    #if defined(MASK_MAP)
        uniform sampler2D u_maskMap;
    #else
        uniform vec4 u_maskColour;
    #endif

        #if defined(BUMP)
        uniform sampler2D u_normalMap;
 
        VARYING_IN vec3 v_tbn[3];
    #else
        VARYING_IN vec3 v_normal;
    #endif

    #if defined(TEXTURED)
        VARYING_IN vec2 v_texCoord;
    #endif
    #if defined (VERTEX_COLOUR)
        VARYING_IN vec4 v_colour;
    #endif
        VARYING_IN vec4 v_fragPosition;

        out vec4[4] o_colour;

        void main()
        {
        #if defined(ALPHA_CLIP)
            if(TEXTURE(u_diffuseMap, v_texCoord).a < u_alphaClip) discard;
        #endif

        #if defined (BUMP)
            vec3 texNormal = TEXTURE(u_normalMap, v_texCoord).rgb * 2.0 - 1.0;
            vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
        #else
            vec3 normal = normalize(v_normal);
        #endif

            vec4 colour = vec4(1.0);
        #if defined(DIFFUSE_MAP)
            colour *= TEXTURE(u_diffuseMap, v_texCoord);
        #endif
        #if defined(COLOURED)
            colour *= u_colour;
        #endif
        #if defined(VERTEX_COLOUR)
            colour *= v_colour;
        #endif

            o_colour[0] = vec4(normal, 1.0);
            o_colour[1] = v_fragPosition;
            o_colour[2] = colour;

        #if defined(MASK_MAP)
            o_colour[3] = TEXTURE(u_maskMap, v_texCoord);
        #else
            o_colour[3] = u_maskColour;
        #endif

        })";

    inline const std::string LightingVertex = 
        R"(
            ATTRIBUTE vec4 a_position;

            uniform mat4 u_worldMatrix;
            uniform mat4 u_projectionMatrix;
            
            VARYING_OUT vec2 v_texCoord;

            void main()
            {
                gl_Position = u_projectionMatrix * u_worldMatrix * a_position;
                v_texCoord = a_position.xy;
            }
        )";
    
    inline const std::string LightingFragment = 
        R"(
            uniform sampler2D u_diffuseMap;
            uniform sampler2D u_maskMap;
            uniform sampler2D u_normalMap;
            uniform sampler2D u_positionMap;

            uniform sampler2D u_shadowMap;
            uniform mat4 u_inverseViewMatrix;
            uniform mat4 u_lightProjectionMatrix;

            uniform vec3 u_lightDirection;
            uniform vec4 u_lightColour;
            uniform vec3 u_cameraWorldPosition;

            uniform samplerCube u_irradianceMap;
            uniform samplerCube u_prefilterMap;
            uniform sampler2D u_brdfMap;

            VARYING_IN vec2 v_texCoord;

            out vec4 o_colour;

            //need to define these before shadow calc :3
            struct MaterialProperties
            {
                vec3 albedo;
                float metallic;
                float roughness;
                float ao;
            };

            struct SurfaceProperties
            {
                vec3 viewDir;
                vec3 normalDir;
                vec3 lightDir;
            };


            const vec2 kernel[16] = vec2[](
                vec2(-0.94201624, -0.39906216),
                vec2(0.94558609, -0.76890725),
                vec2(-0.094184101, -0.92938870),
                vec2(0.34495938, 0.29387760),
                vec2(-0.91588581, 0.45771432),
                vec2(-0.81544232, -0.87912464),
                vec2(-0.38277543, 0.27676845),
                vec2(0.97484398, 0.75648379),
                vec2(0.44323325, -0.97511554),
                vec2(0.53742981, -0.47373420),
                vec2(-0.26496911, -0.41893023),
                vec2(0.79197514, 0.19090188),
                vec2(-0.24188840, 0.99706507),
                vec2(-0.81409955, 0.91437590),
                vec2(0.19984126, 0.78641367),
                vec2(0.14383161, -0.14100790)
            );
            const int filterSize = 3;
            float shadowAmount(vec4 lightWorldPos, SurfaceProperties surfProp)
            {
                vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
                projectionCoords = projectionCoords * 0.5 + 0.5;

                if(projectionCoords.z > 1.0) return 1.0;

                float slope = dot(surfProp.normalDir, surfProp.lightDir);

                float bias = max(0.008 * (1.0 - slope), 0.001);
                //float bias = 0.004;

                float shadow = 0.0;
                vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
                for(int x = 0; x < filterSize; ++x)
                {
                    for(int y = 0; y < filterSize; ++y)
                    {
                        float pcfDepth = TEXTURE(u_shadowMap, projectionCoords.xy + kernel[y * filterSize + x] * texelSize).r;
                        shadow += (projectionCoords.z - bias) > pcfDepth ? 0.4 : 0.0;
                    }
                }
                return 1.0 - ((shadow / 9.0) * clamp(slope, 0.0, 1.0));
            }


            const float PI = 3.14159265359;
            float distributionGGX(vec3 N, vec3 H, float roughness)
            {
                float a = roughness * roughness;
                float a2 = a * a;
                float NdotH = max(dot(N, H), 0.0);
                float NdotH2 = NdotH * NdotH;

                float nom   = a2;
                float denom = (NdotH2 * (a2 - 1.0) + 1.0);
                denom = PI * denom * denom;

                return nom / denom;
            }

            float geometrySchlickGGX(float NdotV, float roughness)
            {
                float r = (roughness + 1.0);
                float k = (r * r) / 8.0;

                float nom   = NdotV;
                float denom = NdotV * (1.0 - k) + k;

                return nom / denom;
            }

            float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
            {
                float NdotV = max(dot(N, V), 0.0);
                float NdotL = max(dot(N, L), 0.0);
                float ggx2 = geometrySchlickGGX(NdotV, roughness);
                float ggx1 = geometrySchlickGGX(NdotL, roughness);

                return ggx1 * ggx2;
            }

            vec3 fresnelSchlick(float cosTheta, vec3 F0)
            {
                return F0 + (1.0 - F0) * pow(1.0 - min(cosTheta, 1.0), 5.0);
            }        

            vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
            {
                return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - min(cosTheta, 1.0), 5.0);
            }

            vec3 calcLighting(MaterialProperties matProp, SurfaceProperties surfProp, vec3 lightColour, vec3 F0)
            {
                vec3 halfDir = normalize(surfProp.viewDir + surfProp.lightDir);
                vec3 radiance = lightColour;

                //Cook-Torrance BRDF
                float NDF = distributionGGX(surfProp.normalDir, halfDir, matProp.roughness);   
                float G   = geometrySmith(surfProp.normalDir, surfProp.viewDir, surfProp.lightDir, matProp.roughness);      
                vec3 F    = fresnelSchlick(max(dot(halfDir, surfProp.viewDir), 0.0), F0);
           
                vec3 nominator = NDF * G * F; 
                float denominator = 4 * max(dot(surfProp.normalDir, surfProp.viewDir), 0.0) * max(dot(surfProp.normalDir, surfProp.lightDir), 0.0) + 0.001; // 0.001 to prevent divide by zero.
                vec3 specular = nominator / denominator;
        
                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - matProp.metallic;	  

                //scale light by NdotL
                float NdotL = max(dot(surfProp.normalDir, surfProp.lightDir), 0.0);        

                return (kD * matProp.albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
            }

            void main()
            {
                MaterialProperties matProp;
                SurfaceProperties surfProp;

                surfProp.normalDir = normalize((u_inverseViewMatrix * vec4(TEXTURE(u_normalMap, v_texCoord).rgb, 0.0)).xyz);
                
                vec4 position = TEXTURE(u_positionMap, v_texCoord);
                //surfProp.viewDir = normalize(u_cameraWorldPosition - (u_inverseViewMatrix * vec4(position.rgb, 0.0)).xyz);
                surfProp.viewDir = normalize((u_inverseViewMatrix * vec4(-position.rgb, 0.0)).xyz);

                vec4 albedo = TEXTURE(u_diffuseMap, v_texCoord);
                matProp.albedo = albedo.rgb;
                matProp.albedo = pow(matProp.albedo, vec3(2.2)); //gamma

                vec4 mask = TEXTURE(u_maskMap, v_texCoord);
                matProp.metallic = mask.r;
                matProp.roughness = mask.g,
                matProp.ao = mask.b;
                //TODO emission
            

                vec3 F0 = vec3(0.04); 
                F0 = mix(F0, matProp.albedo, matProp.metallic);

                vec3 Lo = vec3(0.0);

                //point lights
                /*
                for( int i = 0; i < POINT_LIGHT_COUNT; ++i)
                {
                    vec3 lightDir = u_pointLights[i].worldPos - v_worldPosition;
                    surfProp.lightDir = normalize(lightDir);

                    float distance = length(lightDir);
                    float attenuation = 1.0 / (distance * distance);
                    Lo += calcLighting(matProp, surfProp, u_pointLights[i].colour.rgb * attenuation, F0);
                }
                */

                //directional light
                surfProp.lightDir = normalize(-u_lightDirection);
                Lo += calcLighting(matProp, surfProp, u_lightColour.rgb, F0);

                //ambient lighting
                vec3 kS = fresnelSchlickRoughness(max(dot(surfProp.normalDir, surfProp.viewDir), 0.0), F0, matProp.roughness);
                vec3 kD = 1.0 - kS;
                kD *= 1.0 - matProp.metallic;

                vec3 irradiance = TEXTURE(u_irradianceMap, surfProp.normalDir).rgb;
                vec3 diffuse = irradiance * matProp.albedo;

                //specular
                const float MAX_REFLECTION_LOD = 4.0;
                vec3 prefilteredColor = textureLod(u_prefilterMap, reflect(-surfProp.viewDir, surfProp.normalDir),  matProp.roughness * MAX_REFLECTION_LOD).rgb;    
                vec2 brdf  = TEXTURE(u_brdfMap, vec2(max(dot(surfProp.normalDir, surfProp.viewDir), 0.0), matProp.roughness)).rg;
                vec3 specular = prefilteredColor * (kS * brdf.x + brdf.y);

                vec3 ambient = (kD * diffuse + specular) * matProp.ao;
    
                vec3 colour = ambient + Lo;

                //HDR tonemapping
                colour = colour / (colour + vec3(1.0));
                //gamma correct
                colour = pow(colour, vec3(1.0 / 2.2)); 

                vec4 lightPos = u_lightProjectionMatrix * position; //undoes the view position too
                colour *= shadowAmount(lightPos, surfProp);

                colour *= u_lightColour.rgb;

                o_colour = vec4(colour, albedo.a);
            }
        )";


    inline const std::string OITShadedFragment = 
        R"(
            out vec4[6] o_outColour;

uniform mat4 u_viewMatrix;

        #if defined(DIFFUSE_MAP)
            uniform sampler2D u_diffuseMap;

        #if defined(ALPHA_CLIP)
            uniform float u_alphaClip;
        #endif
        #endif

        #if defined(MASK_MAP)
            uniform sampler2D u_maskMap;
        #else
            uniform LOW vec4 u_maskColour;
        #endif

        #if defined(BUMP)
            uniform sampler2D u_normalMap;
        #endif

            uniform samplerCube u_skybox;

            uniform HIGH vec3 u_lightDirection;
            uniform LOW vec4 u_lightColour;
            uniform HIGH vec3 u_cameraWorldPosition;
                
        #if defined(COLOURED)
            uniform LOW vec4 u_colour;
        #endif

        #if defined (RX_SHADOWS)
            uniform sampler2D u_shadowMap;
        #endif

            VARYING_IN HIGH vec3 v_worldPosition;
        #if defined(VERTEX_COLOUR)
            VARYING_IN LOW vec4 v_colour;
        #endif
        #if defined (BUMP)
            VARYING_IN HIGH vec3 v_tbn[3];
        #else
            VARYING_IN HIGH vec3 v_normalVector;
        #endif
        #if defined(TEXTURED)
            VARYING_IN MED vec2 v_texCoord0;
        #endif
 
        #if defined(RX_SHADOWS)
            VARYING_IN LOW vec4 v_lightWorldPosition;

        #define PREC

            //some fancier pcf on desktop
            const vec2 kernel[16] = vec2[](
                vec2(-0.94201624, -0.39906216),
                vec2(0.94558609, -0.76890725),
                vec2(-0.094184101, -0.92938870),
                vec2(0.34495938, 0.29387760),
                vec2(-0.91588581, 0.45771432),
                vec2(-0.81544232, -0.87912464),
                vec2(-0.38277543, 0.27676845),
                vec2(0.97484398, 0.75648379),
                vec2(0.44323325, -0.97511554),
                vec2(0.53742981, -0.47373420),
                vec2(-0.26496911, -0.41893023),
                vec2(0.79197514, 0.19090188),
                vec2(-0.24188840, 0.99706507),
                vec2(-0.81409955, 0.91437590),
                vec2(0.19984126, 0.78641367),
                vec2(0.14383161, -0.14100790)
            );
            const int filterSize = 3;
            float shadowAmount(vec4 lightWorldPos)
            {
                vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
                projectionCoords = projectionCoords * 0.5 + 0.5;

                if(projectionCoords.z > 1.0) return 1.0;

                float shadow = 0.0;
                vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
                for(int x = 0; x < filterSize; ++x)
                {
                    for(int y = 0; y < filterSize; ++y)
                    {
                        float pcfDepth = TEXTURE(u_shadowMap, projectionCoords.xy + kernel[y * filterSize + x] * texelSize).r;
                        shadow += (projectionCoords.z - 0.001) > pcfDepth ? 0.4 : 0.0;
                    }
                }
                return 1.0 - (shadow / 9.0);
            }
        #endif               

            LOW vec4 diffuseColour = vec4(1.0);
            HIGH vec3 eyeDirection;
            LOW vec4 mask = vec4(1.0, 1.0, 0.0, 1.0);
            vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
            {
                MED float diffuseAmount = max(dot(normal, lightDirection), 0.0);
                //diffuseAmount = pow((diffuseAmount * 0.5) + 5.0, 2.0);
                MED vec3 mixedColour = diffuseColour.rgb * lightDiffuse * diffuseAmount * falloff;

                MED vec3 halfVec = normalize(eyeDirection + lightDirection);
                MED float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
                LOW vec3 specularColour = lightSpecular * vec3(pow(specularAngle, ((254.0 * mask.r) + 1.0))) * falloff;

                return clamp(mixedColour + (specularColour * mask.g), 0.0, 1.0);
            }

            void main()
            {
            #if defined (BUMP)
                MED vec3 texNormal = TEXTURE(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
                MED vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
            #else
                MED vec3 normal = normalize(v_normalVector);
            #endif

            #if defined (DIFFUSE_MAP)
                diffuseColour *= TEXTURE(u_diffuseMap, v_texCoord0);

            #if defined(ALPHA_CLIP)
                if(diffuseColour.a < u_alphaClip) discard;
            #endif
            #endif

            #if defined(MASK_MAP)
                mask = TEXTURE(u_maskMap, v_texCoord0);
            #else
                mask = u_maskColour;
            #endif

            #if defined(COLOURED)
                diffuseColour *= u_colour;
            #endif
                
            #if defined(VERTEX_COLOUR)
                diffuseColour *= v_colour;
            #endif
                LOW vec3 blendedColour = diffuseColour.rgb * 0.2; //ambience
                eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

                blendedColour += calcLighting(normal, normalize(-u_lightDirection), u_lightColour.rgb, vec3(1.0), 1.0);
            #if defined (RX_SHADOWS)
                blendedColour *= shadowAmount(v_lightWorldPosition);
            #endif

                vec4 finalColour = vec4(1.0);

                finalColour.rgb = mix(blendedColour, diffuseColour.rgb, mask.b);
                finalColour.a = diffuseColour.a;

                vec3 I = normalize(v_worldPosition - u_cameraWorldPosition);
                vec3 R = reflect(I, normal);
                finalColour.rgb = mix(TEXTURE_CUBE(u_skybox, R).rgb, finalColour.rgb, mask.a);

                float weight = clamp(pow(min(1.0, finalColour.a * 10.0) + 0.01, 3.0) * 1e8 * 
                                             pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3);
                finalColour.rgb *= finalColour.a;
                o_outColour[4] = finalColour * weight;
                o_outColour[5].r = finalColour.a;

o_outColour[0].rgb = (u_viewMatrix * vec4(normal, 0.0)).xyz;
o_outColour[1].rgb = (u_viewMatrix * vec4(v_worldPosition, 1.0)).xyz;
        })";

    inline const std::string OITUnlitFragment = 
        R"(
            out vec4[6] o_outColour;

        #if defined (TEXTURED)
            uniform sampler2D u_diffuseMap;
        #if defined(ALPHA_CLIP)
            uniform float u_alphaClip;
        #endif
        #endif
        
        #if defined(COLOURED)
            uniform LOW vec4 u_colour;
        #endif
        
        #if defined (RX_SHADOWS)
            uniform sampler2D u_shadowMap;
        #endif

        #if defined (VERTEX_COLOUR)
            VARYING_IN LOW vec4 v_colour;
        #endif
        #if defined (TEXTURED)
            VARYING_IN MED vec2 v_texCoord0;
        #endif

        #if defined(RX_SHADOWS)
            VARYING_IN LOW vec4 v_lightWorldPosition;

        #define PREC
            //some fancier pcf on desktop
            const vec2 kernel[16] = vec2[](
                vec2(-0.94201624, -0.39906216),
                vec2(0.94558609, -0.76890725),
                vec2(-0.094184101, -0.92938870),
                vec2(0.34495938, 0.29387760),
                vec2(-0.91588581, 0.45771432),
                vec2(-0.81544232, -0.87912464),
                vec2(-0.38277543, 0.27676845),
                vec2(0.97484398, 0.75648379),
                vec2(0.44323325, -0.97511554),
                vec2(0.53742981, -0.47373420),
                vec2(-0.26496911, -0.41893023),
                vec2(0.79197514, 0.19090188),
                vec2(-0.24188840, 0.99706507),
                vec2(-0.81409955, 0.91437590),
                vec2(0.19984126, 0.78641367),
                vec2(0.14383161, -0.14100790)
            );
            const int filterSize = 3;
            float shadowAmount(vec4 lightWorldPos)
            {
                vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
                projectionCoords = projectionCoords * 0.5 + 0.5;

                if(projectionCoords.z > 1.0) return 1.0;

                float shadow = 0.0;
                vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
                for(int x = 0; x < filterSize; ++x)
                {
                    for(int y = 0; y < filterSize; ++y)
                    {
                        float pcfDepth = TEXTURE(u_shadowMap, projectionCoords.xy + kernel[y * filterSize + x] * texelSize).r;
                        shadow += (projectionCoords.z - 0.001) > pcfDepth ? 0.4 : 0.0;
                    }
                }
                return 1.0 - (shadow / 9.0);
            }
            #endif

            void main()
            {
            #if defined (VERTEX_COLOUR)
                vec4 finalColour = v_colour;
            #else
                vec4 finalColour = vec4(1.0);
            #endif
            #if defined (TEXTURED)
                finalColour *= TEXTURE(u_diffuseMap, v_texCoord0);
            #if defined(ALPHA_CLIP)
                if(finalColour.a < u_alphaClip) discard;
            #endif
            #endif

            #if defined(COLOURED)
                finalColour *= u_colour;
            #endif

            #if defined (RX_SHADOWS)
                finalColour.rgb *= shadowAmount(v_lightWorldPosition);
            #endif

            float weight = clamp(pow(min(1.0, finalColour.a * 10.0) + 0.01, 3.0) * 1e8 * 
                                   pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3);

/*
float weight =
    max(min(1.0, max(max(finalColour.r, finalColour.g), finalColour.b) * finalColour.a), finalColour.a) *
            clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 200, 4.0)), 1e-2, 3e3);
*/

            o_outColour[4] = vec4(finalColour.rgb * finalColour.a, finalColour.a) * weight;
            o_outColour[5].r = finalColour.a;
        })";

    inline const std::string OITOutputFragment =
        R"(
            OUTPUT

            uniform sampler2D u_accumMap;
            uniform sampler2D u_revealMap;

            VARYING_IN vec2 v_texCoord;

            const float Epsilon = 0.00001;

            bool approxEqual(float a, float b)
            {
                return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * Epsilon;
            }

            float max3(vec3 v)
            {
                return max(max(v.x, v.y), v.z);
            }

            void main()
            {
                float revealage = TEXTURE(u_revealMap, v_texCoord).r;
                
                if(approxEqual(revealage, 1.0))
                {
                    discard;
                }

                vec4 accum = TEXTURE(u_accumMap, v_texCoord);

                if(isinf(max3(abs(accum.rgb))))
                {
                    accum.rgb = vec3(accum.a);
                }

                vec3 colour = accum.rgb / max(accum.a, Epsilon);

                FRAG_OUT = vec4(colour, 1.0 - revealage);
            }
        )";
}