/*
PBR Lighting calculations based on Joey DeVries OpenGL tutorial
https://learnopengl.com/PBR

*/

#pragma once

#include <string>

namespace cro::Shaders::PBR
{
    //note that this shader is not designed with mobile
    //devices in mind. It also shares a vertex shader
    //with the VertexLit type.
    inline const std::string Fragment = 
        R"(
    layout (location = 0) out vec4 FRAG_OUT;
    layout (location = 1) out vec4 NORM_OUT;
    layout (location = 2) out vec4 POS_OUT;
uniform mat4 u_viewMatrix;

        #if defined(DIFFUSE_MAP)
        uniform sampler2D u_diffuseMap;
        #endif
        uniform vec4 u_colour;

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
        #endif

        #if defined (RX_SHADOWS)
#include SHADOWMAP_UNIFORMS_FRAG
        #endif

        uniform vec3 u_lightDirection;
        uniform vec4 u_lightColour;
        uniform vec3 u_cameraWorldPosition;

        uniform samplerCube u_irradianceMap;
        uniform samplerCube u_prefilterMap;
        uniform sampler2D u_brdfMap;

        VARYING_IN vec3 v_worldPosition;

        #if defined (VERTEX_COLOUR)
        VARYING_IN vec4 v_colour;
        #endif

        #if defined(BUMP)
        VARYING_IN vec3 v_tbn[3];
        #else
        VARYING_IN vec3 v_normalVector;
        #endif

        #if defined(TEXTURED)
        VARYING_IN vec2 v_texCoord0;
        #endif

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

        #if defined(RX_SHADOWS)
#include SHADOWMAP_INPUTS


#define PBR //affects the shadow include, below
#include PCF_SHADOWS

        #endif


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

        #if defined(BUMP)
            vec3 texNormal = TEXTURE(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
            surfProp.normalDir = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
        #else
            surfProp.normalDir = normalize(v_normalVector);
        #endif

//NORM_OUT = vec4(surfProp.normalDir, 1.0);
//POS_OUT = vec4(v_worldPosition, 1.0);

NORM_OUT = u_viewMatrix * vec4(surfProp.normalDir, 0.0);
NORM_OUT.a = 1.0;
POS_OUT = u_viewMatrix * vec4(v_worldPosition, 1.0);

            surfProp.viewDir = normalize(u_cameraWorldPosition - v_worldPosition);

        #if defined(DIFFUSE_MAP)
            vec4 albedo = TEXTURE(u_diffuseMap, v_texCoord0);

            #if defined(ALPHA_CLIP)
            if(albedo.a < u_alphaClip) discard;
            #endif

            matProp.albedo = albedo.rgb;
            matProp.albedo *= u_colour.rgb;
        #else
            matProp.albedo = u_colour.rgb;
        #endif

        #if defined(VERTEX_COLOUR)
            matProp.albedo *= v_colour.rgb;
        #endif

            matProp.albedo = pow(matProp.albedo, vec3(2.2));

        #if defined(MASK_MAP)
            vec3 mask = TEXTURE(u_maskMap, v_texCoord0).rgb;
        #else
            vec3 mask = u_maskColour.rgb;
        #endif
            matProp.metallic = mask.r;
            matProp.roughness = mask.g;
            matProp.ao = mask.b;


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
            

        #if defined (RX_SHADOWS)

            int cascadeIndex = getCascadeIndex();
            float shadow = shadowAmount(cascadeIndex, surfProp);
            float fade = smoothstep(u_frustumSplits[cascadeIndex] + 0.5, u_frustumSplits[cascadeIndex],  v_viewDepth);
            if(fade > 0)
            {
                int nextIndex = min(cascadeIndex + 1, u_cascadeCount - 1);
                shadow = mix(shadow, shadowAmount(nextIndex, surfProp), fade);
            }

            colour *= shadow;

//highlights the cascade splits
//vec3 Colours[4] = vec3[4](vec3(0.2,0.0,0.0), vec3(0.0,0.2,0.0),vec3(0.0,0.0,0.2),vec3(0.2,0.0,0.2));
//for(int i = 0; i < u_cascadeCount; ++i)
//{
//    if (v_lightWorldPosition[i].w > 0.0)
//    {
//        vec2 coords = v_lightWorldPosition[i].xy / v_lightWorldPosition[i].w / 2.0 + 0.5;
//        if (coords.x > 0 && coords.x < 1 
//                && coords.y > 0 && coords.y < 1)
//        {
//              colour += Colours[i];
//        }
//    }
//}
        #endif

            colour *= u_lightColour.rgb;

            FRAG_OUT = vec4(colour, 1.0);
        })";
}