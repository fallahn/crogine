/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

static const std::string TerrainVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec3 a_tangent; //dest position
    ATTRIBUTE vec3 a_bitangent; //dest normal

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

#if defined(RX_SHADOWS)
    uniform mat4 u_lightViewProjectionMatrix;
#endif

    uniform vec4 u_clipPlane;
    uniform float u_morphTime;
    uniform vec3 u_cameraWorldPosition;

    layout (std140) uniform ScaledResolution
    {
        vec2 u_scaledResolution;
        float u_fadeDistance;
    };

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT vec3 v_cameraWorldPosition;

#if defined(RX_SHADOWS)
    VARYING_OUT LOW vec4 v_lightWorldPosition;
#endif

    vec3 lerp(vec3 a, vec3 b, float t)
    {
        return a + ((b - a) * t);
    }

    void main()
    {
        v_cameraWorldPosition = u_cameraWorldPosition;

        vec4 position = u_worldMatrix * vec4(lerp(a_position.xyz, a_tangent, u_morphTime), 1.0);
        //gl_Position = u_viewProjectionMatrix * position;

        vec4 vertPos = u_viewProjectionMatrix * position;
        vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;
        gl_Position = vertPos;

    #if defined (RX_SHADOWS)
        v_lightWorldPosition = u_lightViewProjectionMatrix * position;
    #endif

        //this should be a slerp really but lerp is good enough for low spec shenanigans
        v_normal = u_normalMatrix * lerp(a_normal, a_bitangent, u_morphTime);
        v_colour = a_colour;
        v_worldPosition = position.xyz;

        gl_ClipDistance[0] = dot(position, u_clipPlane);
    })";

static const std::string SlopeVertexShader =
R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform vec3 u_centrePosition;
    uniform vec3 u_cameraWorldPosition;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;

    const float Radius = 5.0;

    void main()
    {
        vec4 worldPos = u_worldMatrix * a_position;
        float alpha = 1.0 - smoothstep(Radius, Radius + 5.0, length(worldPos.xyz - u_centrePosition));
        alpha *= (1.0 - smoothstep(Radius, Radius + 1.0, length(worldPos.xyz - u_cameraWorldPosition)));

        gl_Position = u_viewProjectionMatrix * worldPos;

        v_colour = a_colour;
        v_colour.a *= alpha;

        v_texCoord = a_texCoord0;
    }   
)";

//the UV coords actually indicate direction of slope
static const std::string SlopeFragmentShader =
R"(
    OUTPUT

    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };    
    uniform float u_alpha;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    //const float Scale = 20.0;

    void main()
    {
        float alpha = (sin(v_texCoord.x - ((u_windData.w * 10.f) * v_texCoord.y)) + 1.0) * 0.5;
        alpha = step(0.5, alpha);

        FRAG_OUT = v_colour;
        FRAG_OUT.a *= alpha * u_alpha;
    }
)";

static const std::string CelVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
#if defined (TEXTURED)
    ATTRIBUTE vec2 a_texCoord0;
#endif
#if defined (VATS)
    ATTRIBUTE vec2 a_texCoord1;
#endif

#if defined(INSTANCING)
    ATTRIBUTE mat4 a_instanceWorldMatrix;
    ATTRIBUTE mat3 a_instanceNormalMatrix;
#endif

#if defined(SKINNED)
    #define MAX_BONES 64
    ATTRIBUTE vec4 a_boneIndices;
    ATTRIBUTE vec4 a_boneWeights;
    uniform mat4 u_boneMatrices[MAX_BONES];
#endif

#if defined (VATS)
    #define MAX_INSTANCE 3
    uniform sampler2D u_vatsPosition;
    uniform sampler2D u_vatsNormal;
    uniform float u_time;
    uniform float u_maxTime;
    uniform float u_offsetMultiplier;
#endif

#if defined(RX_SHADOWS)
    uniform mat4 u_lightViewProjectionMatrix;
#endif

#if defined(INSTANCING)
    uniform mat4 u_viewMatrix;
#else
    uniform mat4 u_worldViewMatrix;
    uniform mat3 u_normalMatrix;
#endif
    uniform mat4 u_worldMatrix;
    uniform mat4 u_projectionMatrix;

    uniform vec4 u_clipPlane;
    uniform vec3 u_cameraWorldPosition;

    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };

    layout (std140) uniform ScaledResolution
    {
        vec2 u_scaledResolution;
        float u_nearFadeDistance;
    };

    VARYING_OUT float v_ditherAmount;
    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec3 v_cameraWorldPosition;
    VARYING_OUT vec3 v_worldPosition;

#if defined (TEXTURED)
    VARYING_OUT vec2 v_texCoord;
#endif

#if defined(RX_SHADOWS)
    VARYING_OUT LOW vec4 v_lightWorldPosition;
#endif

#if defined (NORMAL_MAP)
    VARYING_OUT vec2 v_normalTexCoord;
    const vec2 MapSize = vec2(320.0, 200.0);
#endif

    vec3 decodeVector(sampler2D source, vec2 coord)
    {
        vec3 vec = TEXTURE(source, coord).rgb;
        vec *= 2.0;
        vec -= 1.0;

        return vec;
    }

    void main()
    {
    #if defined (INSTANCING)
        mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
        mat4 worldViewMatrix = u_viewMatrix * worldMatrix;
        mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;            
    #else
        mat4 worldMatrix = u_worldMatrix;
        mat4 worldViewMatrix = u_worldViewMatrix;
        mat3 normalMatrix = u_normalMatrix;
    #endif

    #if defined (VATS)
        vec2 texCoord = a_texCoord1;
        float scale = texCoord.y;
        float instanceOffset = mod(gl_InstanceID, MAX_INSTANCE) * u_offsetMultiplier;
        texCoord.y = mod(u_time + (0.15 * instanceOffset), u_maxTime);

        vec4 position = vec4(decodeVector(u_vatsPosition, texCoord) * scale, 1.0);
    #else
        vec4 position = a_position;
    #endif

    #if defined(SKINNED)
        mat4 skinMatrix = a_boneWeights.x * u_boneMatrices[int(a_boneIndices.x)];
        skinMatrix += a_boneWeights.y * u_boneMatrices[int(a_boneIndices.y)];
        skinMatrix += a_boneWeights.z * u_boneMatrices[int(a_boneIndices.z)];
        skinMatrix += a_boneWeights.w * u_boneMatrices[int(a_boneIndices.w)];
        position = skinMatrix * position;
    #endif

    #if defined (RX_SHADOWS)
        v_lightWorldPosition = u_lightViewProjectionMatrix * u_worldMatrix * position;
    #endif


#if defined(WIND_WARP)
#if !defined(WOBBLE)
        const float xFreq = 0.6;
        //float xFreq = 0.6 + (3.4 * a_colour.r);
        const float yFreq = 0.8;
        const float scale = 0.2;

        float strength = u_windData.y;
        float totalScale = scale * strength * a_colour.b;

        position.x += sin((u_windData.w * (xFreq)) + worldMatrix[3].x) * totalScale;
        position.z += sin((u_windData.w * (yFreq)) + worldMatrix[3].z) * totalScale;
        position.xz += (u_windData.xz * strength * 2.0) * totalScale;
#endif
#endif

        vec4 worldPosition = worldMatrix * position;
        v_worldPosition = worldPosition.xyz;
        //gl_Position = u_projectionMatrix * worldViewMatrix * position;

        vec4 vertPos = u_projectionMatrix * worldViewMatrix * position;

#if defined(WOBBLE)
        vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;
#endif

        gl_Position = vertPos;

#if defined(VATS)
        vec3 normal = decodeVector(u_vatsNormal, texCoord);
#else
        vec3 normal = a_normal;
#endif
#if defined(SKINNED)
        normal = (skinMatrix * vec4(normal, 0.0)).xyz;
#endif
        v_normal = normalMatrix * normal;
        v_cameraWorldPosition = u_cameraWorldPosition;
        v_colour = a_colour;

#if defined (TEXTURED)
        v_texCoord = a_texCoord0;
#if defined (VATS)
        v_texCoord.x /= MAX_INSTANCE;
        v_texCoord.x += (1.0 / MAX_INSTANCE) * mod(gl_InstanceID, MAX_INSTANCE);
#endif
#endif

#if defined (NORMAL_MAP)
        v_normalTexCoord = vec2(worldPosition.x / MapSize.x, -worldPosition.z / MapSize.y);
#endif
        gl_ClipDistance[0] = dot(worldPosition, u_clipPlane);

#if defined(DITHERED)
        float fadeDistance = u_nearFadeDistance * 2.0;
        const float farFadeDistance = 360.f;
        float distance = length(worldPosition.xyz - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
        v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / fadeDistance, 0.0, 1.0);
#endif
    })";

static const std::string CelFragmentShader = R"(
    uniform vec3 u_lightDirection;

    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };

    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };

#if defined (RX_SHADOWS)
    uniform sampler2D u_shadowMap;
#endif

#if defined (TINT)
    uniform vec4 u_tintColour = vec4(1.0);
#endif

#if defined FADE_INPUT
    uniform float u_fadeAmount = 1.0;
#endif

#if defined (USER_COLOUR)
    uniform vec4 u_hairColour = vec4(1.0, 0.0, 0.0, 1.0);
    uniform vec4 u_darkColour = vec4(0.5);
#endif

#if defined(REFLECTIONS)
    uniform samplerCube u_reflectMap;
#endif

#if defined (SUBRECT)
    uniform vec4 u_subrect = vec4(0.0, 0.0, 1.0, 1.0);
#endif

#if defined (TERRAIN)
    uniform float u_morphTime;
#endif

#if defined (TEXTURED)
    uniform sampler2D u_diffuseMap;
    VARYING_IN vec2 v_texCoord;
#endif

#if defined (CONTOUR)
    uniform float u_transparency;
    uniform float u_minHeight = 0.0;
    uniform float u_maxHeight = 1.0;
#endif

#if defined (NORMAL_MAP)
    uniform sampler2D u_normalMap;
    VARYING_IN vec2 v_normalTexCoord;
#endif

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_colour;
    VARYING_IN float v_ditherAmount;
    VARYING_IN vec3 v_cameraWorldPosition;
    VARYING_IN vec3 v_worldPosition;

    OUTPUT

#if defined(RX_SHADOWS)
    VARYING_IN LOW vec4 v_lightWorldPosition;

    const float Bias = 0.001; //0.005
    float shadowAmount(LOW vec4 lightWorldPos)
    {
        vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
        projectionCoords = projectionCoords * 0.5 + 0.5;
        float depthSample = TEXTURE(u_shadowMap, projectionCoords.xy).r;
        float currDepth = projectionCoords.z - Bias;

        float featherX = smoothstep(0.0, 0.05, projectionCoords.x);
        featherX *= (1.0 - smoothstep(0.95, 1.0, projectionCoords.x));

        float featherY = smoothstep(0.0, 0.05, projectionCoords.y);
        featherY *= (1.0 - smoothstep(0.95, 1.0, projectionCoords.y));


        if (currDepth > 1.0)
        {
            return 1.0;
        }

        return (currDepth < depthSample) ? 1.0 : 1.0 - (0.3 * featherX * featherY);
    }

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
    float shadowAmountSoft(vec4 lightWorldPos)
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

        float featherX = smoothstep(0.0, 0.05, projectionCoords.x);
        featherX *= (1.0 - smoothstep(0.95, 1.0, projectionCoords.x));

        float featherY = smoothstep(0.0, 0.05, projectionCoords.y);
        featherY *= (1.0 - smoothstep(0.95, 1.0, projectionCoords.y));


        float amount = shadow / 9.0;
        return 1.0 - (amount * featherX * featherY);
    }
#endif

#if defined (ADD_NOISE)
    uniform vec4 u_noiseColour = vec4(0.0,0.0,0.0,1.0);

    const float NoisePerMetre = 10.0;
    float rand(float n){return fract(sin(n) * 43758.5453123);}

    float noise(vec2 pos)
    {
        return fract(sin(dot(pos, vec2(12.9898, 4.1414))) * 43758.5453);
    }
#endif


    //function based on example by martinsh.blogspot.com
    const int MatrixSize = 8;
    float findClosest(int x, int y, float c0)
    {
        /* 8x8 Bayer ordered dithering */
        /* pattern. Each input pixel */
        /* is scaled to the 0..63 range */
        /* before looking in this table */
        /* to determine the action. */

        const int dither[64] = int[64](
         0, 32, 8, 40, 2, 34, 10, 42, 
        48, 16, 56, 24, 50, 18, 58, 26, 
        12, 44, 4, 36, 14, 46, 6, 38, 
        60, 28, 52, 20, 62, 30, 54, 22, 
         3, 35, 11, 43, 1, 33, 9, 41, 
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47, 7, 39, 13, 45, 5, 37,
        63, 31, 55, 23, 61, 29, 53, 21 );

        float limit = 0.0;
        if (x < MatrixSize)
        {
            limit = (dither[y * MatrixSize + x] + 1) / 64.0;
        }

        if (c0 < limit)
        {
            return 0.0;
        }
        return 1.0;
    }
    //https://gist.github.com/yiwenl/745bfea7f04c456e0101
    vec3 rgb2hsv(vec3 c)
    {
        vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
        vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
        vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

        float d = q.x - min(q.w, q.y);
        float e = 1.0e-10;
        return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
    }

    vec3 hsv2rgb(vec3 c)
    {
        vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }

    vec3 complementaryColour(vec3 c)
    {
        vec3 a = rgb2hsv(c);
        a.x += 0.5;
        c = hsv2rgb(a);
        return c;
    }


    const float Quantise = 10.0;
    const float TerrainLevel = -0.049;
    const float WaterLevel = -0.019;

    void main()
    {
#if defined(TERRAIN)
    //if (v_worldPosition.y < TerrainLevel - u_morphTime) discard;
#endif

        vec4 colour = vec4(1.0);
#if defined (TEXTURED)
        vec2 texCoord = v_texCoord;

#if defined (SUBRECT)
        texCoord *= u_subrect.ba;
        texCoord += u_subrect.rg;
#endif

        vec4 c = TEXTURE(u_diffuseMap, texCoord);

        if(c. a < 0.2)
        {
            discard;
        }
        colour *= c;
#endif
#if defined (VERTEX_COLOURED)
        colour *= v_colour;
#endif

#if defined (NORMAL_MAP)
        //colour = TEXTURE(u_normalMap, v_normalTexCoord);
        //colour.rg = v_normalTexCoord;
        vec3 normal = TEXTURE(u_normalMap, v_normalTexCoord).rgb * 2.0 - 1.0;
#else
        vec3 normal = normalize(v_normal);
#endif
        vec3 lightDirection = normalize(-u_lightDirection);
        float amount = dot(normal, lightDirection);

#if defined (USER_COLOUR)
        //colour *= mix(u_darkColour, u_hairColour, step(0.5, amount));
        colour *= u_hairColour;
#endif

        //float checkAmount = step(0.3, 1.0 - amount);
        float checkAmount = smoothstep(0.3, 0.9, 1.0 - amount);

        amount *= 2.0;
        amount = round(amount);
        amount /= 2.0;
        //colour.rgb = mix(complementaryColour(colour.rgb), colour.rgb, (amount * 0.4) + 0.6);

        amount = 0.8 + (amount * 0.2);
        colour.rgb *= amount;

#if !defined(NOCHEX)
        float pixelScale = u_pixelScale;
        //float pixelScale = 1.0;
        float check = mod(floor(gl_FragCoord.x / pixelScale) + floor(gl_FragCoord.y / pixelScale), 2.0) * checkAmount;
        //float check = mod(gl_FragCoord.x + gl_FragCoord.y, 2.0) * checkAmount;
        amount = (1.0 - check) + (check * amount);
        //colour.rgb = complementaryColour(colour.rgb);
        //colour.rgb = mix(complementaryColour(colour.rgb), colour.rgb, amount);
        colour.rgb *= amount;
#endif
        vec3 viewDirection = normalize(v_cameraWorldPosition - v_worldPosition.xyz);
#if defined (SPECULAR)
        vec3 reflectDirection = reflect(-lightDirection, normal);
        float spec = pow(max(dot(viewDirection, reflectDirection), 0.50), 256.0);
        colour.rgb += vec3(spec);
#endif

#if defined(REFLECTIONS)
        colour.rgb += TEXTURE_CUBE(u_reflectMap, reflect(-viewDirection, normal)).rgb * 0.25;
#endif

        FRAG_OUT = vec4(colour.rgb, 1.0);

#if defined (RX_SHADOWS)
        //FRAG_OUT.rgb *= shadowAmountSoft(v_lightWorldPosition);
        FRAG_OUT.rgb *= shadowAmount(v_lightWorldPosition);
        /*if(v_lightWorldPosition.w > 0.0)
        {
            vec2 coords = v_lightWorldPosition.xy / v_lightWorldPosition.w / 2.0 + 0.5;

            float featherX = smoothstep(0.0, 0.05, coords.x);
            featherX *= (1.0 - smoothstep(0.95, 1.0, coords.x));

            float featherY = smoothstep(0.0, 0.05, coords.y);
            featherY *= (1.0 - smoothstep(0.95, 1.0, coords.y));

            if(coords.x>0&&coords.x<1&&coords.y>0&&coords.y<1)
            FRAG_OUT.rgb += vec3(0.0,0.0,0.5 * featherX * featherY);
        }*/
#endif

#if defined (ADD_NOISE)
        float noiseVal = noise(floor(v_worldPosition.xz * NoisePerMetre)) * 0.4;
        //noiseVal *= (noise(floor(v_worldPosition.xz / 6.0)) * 0.3);
        //FRAG_OUT.rgb *= (1.0 - noiseVal);
        FRAG_OUT.rgb = mix(FRAG_OUT.rgb, u_noiseColour.rgb, noiseVal);
#endif

#if defined (TINT)
        FRAG_OUT *= u_tintColour;
#endif 

#if defined (TERRAIN)
    //FRAG_OUT.rgb = mix(FRAG_OUT.rgb, vec3(0.02, 0.078, 0.578), clamp(v_worldPosition.y / WaterLevel, 0.0, 1.0));
#endif


#if defined (DITHERED) || defined (FADE_INPUT)// || defined (TERRAIN)
        vec2 xy = gl_FragCoord.xy / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

#if defined (FADE_INPUT)
        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, u_fadeAmount));
//#elif defined(TERRAIN)
//        float alpha = 1.0;
//        if(v_worldPosition.y < WaterLevel)
//        {
//            float distance = clamp(length(vec2(v_worldPosition.xz - vec2(160.0, -100.0))) / 100.0, 0.0, 1.0);
//            alpha = findClosest(x, y, smoothstep(0.9, 0.99, 1.0 - distance));
//        }
#else
        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
#endif
        if(alpha < 0.1) discard;
#endif

#if defined(CONTOUR)
    float height = (v_worldPosition.y - u_minHeight) / (u_maxHeight - u_minHeight);
    vec3 contourColour = mix(vec3(0.0,0.0,1.0), vec3(1.0,0.0,1.0), height);

    vec3 f = fract(v_worldPosition);// * 2.0);
    vec3 df = fwidth(v_worldPosition);// * 2.0);
    vec3 g = step(df * u_pixelScale, f);
    ////vec3 g = smoothstep(df * 1.0, df * 2.0, f);
    //float contour = 1.0 - (g.x * g.y * g.z);

    vec3 faceNormal = normalize(cross(dFdx(v_worldPosition), dFdy(v_worldPosition))) * 20.0;

    float contourX = 1.0 - (g.y * g.z);

    const float frequency = 40.0;
    float dashX = (sin((mod(v_worldPosition.x, 1.0) * frequency) - (u_windData.w * faceNormal.x)) + 1.0) * 0.5;
    contourX *= step(0.25, dashX);

    float contourZ = 1.0 - (g.y * g.x);
    float dashZ = (sin((mod(v_worldPosition.z + 0.5, 1.0) * frequency) - (u_windData.w * faceNormal.z)) + 1.0) * 0.5;
    contourZ *= step(0.25, dashZ);

    vec3 distance = v_worldPosition.xyz - v_cameraWorldPosition;
    float fade = (1.0 - smoothstep(36.0, 81.0, dot(distance, distance))) * u_transparency * 0.75;


    FRAG_OUT.rgb = mix(FRAG_OUT.rgb, contourColour, contourX * fade);
    FRAG_OUT.rgb = mix(FRAG_OUT.rgb, contourColour, contourZ * fade);
    FRAG_OUT.rgb += clamp(height, 0.0, 1.0) * 0.1;
#endif
    })";


const std::string NormalMapVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec3 a_normal;

    uniform mat4 u_projectionMatrix;
    uniform float u_maxHeight;
    uniform float u_lowestPoint;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT float v_height;

    void main()
    {
        gl_Position = u_projectionMatrix * a_position;
        v_normal = a_normal;

        //yeah should be a normal matrix, but YOLO
        float z = v_normal.y;
        v_normal.y = -v_normal.z;
        v_normal.z = z;

        v_height = clamp((a_position.y - u_lowestPoint) / u_maxHeight, 0.0, 1.0);
    }
)";

const std::string NormalMapFragmentShader = R"(
    OUTPUT

    VARYING_IN vec3 v_normal;
    VARYING_IN float v_height;

    void main()
    {
        vec3 normal = normalize(v_normal);
        normal += 1.0;
        normal /= 2.0;

        FRAG_OUT = vec4(normal, v_height);
    }
)";