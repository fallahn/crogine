/*
PBR Lighting calculations based on Joey DeVries OpenGL tutorial
https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/1.2.lighting_textured/1.2.pbr.fs

*/

#pragma once

#include <string>

namespace cro::Shaders::PBR
{
    //note that this shader is not designed with mobile
    //devices in mind. It also shares a vertex shader
    //with the VertexLit type.
    const static std::string Fragment = 
        R"(
        OUTPUT
        #if defined(DIFFUSE_MAP)
        uniform sampler2D u_diffuseMap;
        #endif
        uniform vec4 u_colour;

        #if defined(MASK_MAP)
        uniform sampler2D u_maskMap;
        #else
        uniform vec4 u_maskColour;
        #endif

        #if defined(BUMP)
        uniform sampler2D u_normalMap;
        #endif

        uniform vec3 u_lightDirection;
        uniform vec4 u_lightColour;
        uniform vec3 u_cameraWorldPosition;

        VARYING_IN vec3 v_worldPosition;

        #if defined(BUMP)
        VARYING_IN vec3 v_tbn[3];
        #else
        VARYING_IN vec3 v_normalVector;
        #endif

        #if defined(TEXTURED)
        VARYING_IN vec2 v_texCoord0;
        #endif


        const float PI = 3.14159265359;
        float distributionGGX(vec3 N, vec3 H, float roughness)
        {
            float a = roughness*roughness;
            float a2 = a*a;
            float NdotH = max(dot(N, H), 0.0);
            float NdotH2 = NdotH*NdotH;

            float nom   = a2;
            float denom = (NdotH2 * (a2 - 1.0) + 1.0);
            denom = PI * denom * denom;

            return nom / denom;
        }

        float geometrySchlickGGX(float NdotV, float roughness)
        {
            float r = (roughness + 1.0);
            float k = (r*r) / 8.0;

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

        void main()
        {
        #if defined(BUMP)
            vec3 texNormal = TEXTURE(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
            vec3 normalDir = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
        #else
            vec3 normalDir = normalize(v_normalVector);
        #endif
            vec3 viewDir = normalize(u_cameraWorldPosition - v_worldPosition);

        #if defined(DIFFUSE_MAP)
            vec3 albedo = TEXTURE(u_diffuseMap, v_texCoord0).rgb;
            albedo *= u_colour.rgb;
        #else
            vec3 albedo = u_colour.rgb;
        #endif
            albedo = pow(albedo, vec3(2.2));

        #if defined(MASK_MAP)
            vec3 mask = TEXTURE(u_maskMap, v_texCoord0).rgb;
        #else
            vec3 mask = u_maskColour.rgb;
        #endif
            float metallic = mask.r;
            float roughness = mask.g;
            float ao = mask.b;


            vec3 F0 = vec3(0.04); 
            F0 = mix(F0, albedo, metallic);

            vec3 Lo = vec3(0.0);

            //this would normally be looped for multiple lights
            //here, and ideally farmed out to a function
            //-------------------------------------//
            vec3 lightDir = normalize(-u_lightDirection);
            vec3 halfDir = normalize(viewDir + lightDir);
            
            //this only needs to be attenuated on point lights
            //float distance = length(u_lightDirection);
            //float attenuation = 1.0 / (distance * distance);
            //vec3 radiance = u_lightColour.rgb * attenuation;
            
            vec3 radiance = u_lightColour.rgb;

            //Cook-Torrance BRDF
            float NDF = distributionGGX(normalDir, halfDir, roughness);   
            float G   = geometrySmith(normalDir, viewDir, lightDir, roughness);      
            vec3 F    = fresnelSchlick(max(dot(halfDir, viewDir), 0.0), F0);
           
            vec3 nominator = NDF * G * F; 
            float denominator = 4 * max(dot(normalDir, viewDir), 0.0) * max(dot(normalDir, lightDir), 0.0) + 0.001; // 0.001 to prevent divide by zero.
            vec3 specular = nominator / denominator;
        

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;	  

            //scale light by NdotL
            float NdotL = max(dot(normalDir, lightDir), 0.0);        

            //add to outgoing radiance Lo
            Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
            //-------------------------------------//

            //ambient lighting  - potentially replace this with environment lighting
            vec3 ambient = vec3(0.03) * albedo * ao;
    
            vec3 colour = ambient + Lo;

            //HDR tonemapping
            colour = colour / (colour + vec3(1.0));
            //gamma correct
            colour = pow(colour, vec3(1.0/2.2)); 

            //TODO insert shadow mapping here
            FRAG_OUT = vec4(colour, 1.0);
        })";
}