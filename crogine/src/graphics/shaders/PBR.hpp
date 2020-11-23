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

        void main()
        {
        #if defined(BUMP)
            vec3 texNormal = TEXTURE(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
            vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
        #else
            vec3 normal = normalize(v_normalVector);
        #endif

        #if defined(DIFFUSE_MAP)
            vec3 albedo = TEXTURE(u_diffuseMap, v_texCoord0).rgb;
            albedo *= u_colour.rgb;
        #else
            vec3 albedo = u_colour.rgb;
        #endif

        #if defined(MASK_MAP)
            vec3 mask = TEXTURE(u_maskMap, v_texCoord0).rgb;
        #else
            vec3 mask = u_maskColour.rgb;
        #endif
            float metallic = mask.r;
            float roughness = mask.g;
            float ao = mask.b;


//testy
FRAG_OUT.rgb = albedo;
FRAG_OUT.rgb *= dot(-normal, u_lightDirection);
FRAG_OUT.r *= metallic;
FRAG_OUT.g *= roughness;
FRAG_OUT.b *= ao;
FRAG_OUT.a = 1.0;

        }

        )";
}