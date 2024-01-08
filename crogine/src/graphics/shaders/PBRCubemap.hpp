/*
HDRI pre-filter shaders from https://learnopengl.com/PBR
*/

#pragma once

#include <string>

inline const std::string PBRCubeVertex = R"(
        ATTRIBUTE vec4 a_position;

        VARYING_OUT vec3 v_worldPosition;

        uniform mat4 u_viewProjectionMatrix;

        void main()
        {
            v_worldPosition = a_position.xyz;
            gl_Position =  u_viewProjectionMatrix * a_position;
        })";

inline const std::string HDRToCubeFrag = R"(
        OUTPUT

        uniform sampler2D u_hdrMap;

        VARYING_IN vec3 v_worldPosition;

        const vec2 invAtan = vec2(0.1591, 0.3183);
        vec2 sampleSphericalMap(vec3 v)
        {
            vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
            uv *= invAtan;
            uv += 0.5;
            return uv;
        }

        void main()
        {		
            vec2 uv = sampleSphericalMap(normalize(v_worldPosition));
            vec3 colour = TEXTURE(u_hdrMap, uv).rgb;
    
            FRAG_OUT = vec4(colour, 1.0);
        })";

inline const std::string IrradianceFrag = R"(

        OUTPUT

        uniform samplerCube u_environmentMap;

        VARYING_IN vec3 v_worldPosition;

        const float PI = 3.14159265359;

        void main()
        {		
            vec3 normal = normalize(v_worldPosition);

            vec3 irradiance = vec3(0.0);   
    
            //tangent space calculation from origin point
            vec3 up    = vec3(0.0, 1.0, 0.0);
            vec3 right = cross(up, normal);
            up         = cross(normal, right);
       
            float sampleDelta = 0.025;
            float sampleCount = 0.0;
            for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
            {
                for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
                {
                    vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
                    vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

                    irradiance += TEXTURE(u_environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
                    sampleCount++;
                }
            }
            irradiance = PI * irradiance * (1.0 / float(sampleCount));
    
            FRAG_OUT = vec4(irradiance, 1.0);
        })";

inline const std::string PrefilterFrag = R"(
        OUTPUT

        uniform samplerCube u_environmentMap;
        uniform float u_roughness;

        VARYING_IN vec3 v_worldPosition;

        const float PI = 3.14159265359;
        // ----------------------------------------------------------------------------
        float DistributionGGX(vec3 N, vec3 H, float roughness)
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
        // ----------------------------------------------------------------------------
        // http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
        // efficient VanDerCorpus calculation.
        float RadicalInverse_VdC(uint bits) 
        {
             bits = (bits << 16u) | (bits >> 16u);
             bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
             bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
             bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
             bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
             return float(bits) * 2.3283064365386963e-10; // / 0x100000000
        }
        // ----------------------------------------------------------------------------
        vec2 Hammersley(uint i, uint N)
        {
	        return vec2(float(i)/float(N), RadicalInverse_VdC(i));
        }
        // ----------------------------------------------------------------------------
        vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
        {
	        float a = roughness*roughness;
	
	        float phi = 2.0 * PI * Xi.x;
	        float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	        float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
	        vec3 H;
	        H.x = cos(phi) * sinTheta;
	        H.y = sin(phi) * sinTheta;
	        H.z = cosTheta;
	
	        vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	        vec3 tangent   = normalize(cross(up, N));
	        vec3 bitangent = cross(N, tangent);
	
	        vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	        return normalize(sampleVec);
        }
        // ----------------------------------------------------------------------------
        void main()
        {		
            vec3 normal = normalize(v_worldPosition);
    
            // make the simplyfying assumption that V equals R equals the normal 
            vec3 reflectDir = normal;
            vec3 viewDir = reflectDir;

            const uint SAMPLE_COUNT = 1024u;
            vec3 prefilteredColor = vec3(0.0);
            float totalWeight = 0.0;
    
            for(uint i = 0u; i < SAMPLE_COUNT; ++i)
            {
                //generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
                vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                vec3 halfDir = ImportanceSampleGGX(Xi, normal, u_roughness);
                vec3 lightDir  = normalize(2.0 * dot(viewDir, halfDir) * halfDir - viewDir);

                float NdotL = max(dot(normal, lightDir), 0.0);
                if(NdotL > 0.0)
                {
                    float D = DistributionGGX(normal, halfDir, u_roughness);
                    float NdotH = max(dot(normal, halfDir), 0.0);
                    float HdotV = max(dot(halfDir, viewDir), 0.0);
                    float pdf = D * NdotH / (4.0 * HdotV) + 0.0001; 

                    float resolution = 1024.0; // resolution of source cubemap (per face)
                    float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);
                    float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

                    float mipLevel = u_roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
            
                    prefilteredColor += textureLod(u_environmentMap, lightDir, mipLevel).rgb * NdotL;
                    totalWeight += NdotL;
                }
            }

            prefilteredColor = prefilteredColor / totalWeight;

            FRAG_OUT = vec4(prefilteredColor, 1.0);
        })";