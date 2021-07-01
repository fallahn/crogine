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

static const std::string SSAOFrag = 
R"(
    OUTPUT
    
    #define KERNEL_SIZE 64

    uniform sampler2D u_position;
    uniform sampler2D u_normal;
    uniform sampler2D u_noise;

    uniform vec3 u_samples[KERNEL_SIZE];
    uniform mat4 u_camProjectionMatrix;

    uniform vec2 u_bufferSize;
    uniform vec4 u_bufferViewport;

    VARYING_IN vec2 v_texCoord;

    const float NoiseSize = 4.0;

    void main()
    {
        vec2 noiseScale = u_bufferSize / NoiseSize;

        vec3 position = TEXTURE(u_position, v_texCoord).rgb;
        vec3 normal = normalize(TEXTURE(u_normal, v_texCoord).rgb);
        vec3 randomOffset = normalize(TEXTURE(u_noise, v_texCoord * noiseScale).rgb);

        vec3 tan = normalize(randomOffset - normal * dot(randomOffset, normal));
        vec3 bitan = cross(normal, tan);
        mat3 tbn = mat3(tan, bitan, normal);

        const float radius = 0.5;
        const float epsilon = 0.025;
        float occlusion = 0.0;
        for(int i = 0; i < KERNEL_SIZE; ++i)
        {
            vec3 samplePos = tbn * u_samples[i];
            samplePos = position + samplePos * radius;

            vec4 offset = vec4(samplePos, 1.0);
            offset = u_camProjectionMatrix * offset;
            offset.xyz /= offset.w;
            offset.xy = offset.xy * 0.5 + 0.5;

            offset.xy *= u_bufferViewport.zw;
            offset.xy += u_bufferViewport.xy;

            float depth = TEXTURE(u_position, offset.xy).z;
            float range = smoothstep(0.0, 1.0, radius / abs(position.z - depth));
            occlusion += (depth >= samplePos.z + epsilon ? 1.0 : 0.0) * range;
        }

        occlusion = 1.f - (occlusion / KERNEL_SIZE);
        FRAG_OUT.r = occlusion;
    }
)";


static const std::string BlurFrag =
R"(
    OUTPUT

    uniform sampler2D u_texture;
    uniform vec2 u_bufferSize;

    VARYING_IN vec2 v_texCoord;

    void main()
    {
        vec2 texelSize = 1.0 / u_bufferSize;
        float result = 0.0;

        for (int x = -2; x < 2; ++x) 
        {
            for (int y = -2; y < 2; ++y) 
            {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                result += TEXTURE(u_texture, v_texCoord + offset).r;
            }
        }
        FRAG_OUT.r = result / 16.0;
    }
)";


static const std::string BlendFrag =
R"(
    OUTPUT

    uniform sampler2D u_baseTexture;
    uniform sampler2D u_ssaoTexture;

    VARYING_IN vec2 v_texCoord;

    void main()
    {
        float ao = TEXTURE(u_ssaoTexture, v_texCoord).r;
        vec3 baseColour = TEXTURE(u_baseTexture, v_texCoord).rgb;

        FRAG_OUT = mix(vec4(baseColour, 1.0), vec4(/*baseColour **/ vec3(ao), 1.0), step(v_texCoord.x, 0.5));
    }
)";