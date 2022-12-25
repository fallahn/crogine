/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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
#if !defined(MAX_CASCADES)
#define MAX_CASCADES 3
#endif
    uniform mat4 u_lightViewProjectionMatrix[MAX_CASCADES];
    uniform int u_cascadeCount = 1;
#endif

    uniform mat4 u_viewMatrix;

#if !defined(INSTANCING)
    uniform mat4 u_worldViewMatrix;
    uniform mat3 u_normalMatrix;
#endif
    uniform mat4 u_worldMatrix;
    uniform mat4 u_projectionMatrix;

    uniform vec4 u_clipPlane;
    uniform vec3 u_cameraWorldPosition;

    uniform sampler2D u_noiseTexture;

//dirX, strength, dirZ, elapsedTime
#include WIND_BUFFER

#include RESOLUTION_BUFFER

    VARYING_OUT float v_ditherAmount;
    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec3 v_cameraWorldPosition;
    VARYING_OUT vec3 v_worldPosition;

#if defined (TEXTURED)
    VARYING_OUT vec2 v_texCoord;
#endif

#if defined(RX_SHADOWS)
    VARYING_OUT LOW vec4 v_lightWorldPosition[MAX_CASCADES];
    VARYING_OUT float v_viewDepth;
#endif

#if defined (NORMAL_MAP)
    VARYING_OUT vec2 v_normalTexCoord;
    const vec2 MapSize = vec2(320.0, 200.0);
#endif


#include WIND_CALC

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
        for(int i = 0; i < u_cascadeCount; i++)
        {
            v_lightWorldPosition[i] = u_lightViewProjectionMatrix[i] * worldMatrix * position;
        }
        v_viewDepth = (worldViewMatrix * position).z;
    #endif


#if defined(WIND_WARP)
#if !defined(WOBBLE)
        //red low freq, green high freq, blue direction amount

        WindResult windResult = getWindData(position.xz, worldMatrix[3].xz);
        vec3 vertexStrength = a_colour.rgb;
        //multiply high and low frequency by vertex colours
        windResult.lowFreq *= vertexStrength.r;
        windResult.highFreq *= vertexStrength.g;

        //apply high frequency and low frequency in local space
        position.x += windResult.lowFreq.x + windResult.highFreq.x;
        position.z += windResult.lowFreq.y + windResult.highFreq.y;

        //multiply wind direction by wind strength
        vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * vertexStrength.b;
        //wind dir is added in world space (below)
#endif
#endif

        vec4 worldPosition = worldMatrix * position;

#if defined(WIND_WARP)
#if !defined(WOBBLE)
        worldPosition.xyz += windDir;
#endif
        vec4 vertPos = u_projectionMatrix * u_viewMatrix * worldPosition;
#else
        vec4 vertPos = u_projectionMatrix * worldViewMatrix * position;
#endif
        v_worldPosition = worldPosition.xyz;

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

#include SCALE_BUFFER

//dirX, strength, dirZ, elapsedTime
#include WIND_BUFFER


#if defined (RX_SHADOWS)
#if !defined(MAX_CASCADES)
#define MAX_CASCADES 3
#endif
    uniform sampler2DArray u_shadowMap;
    uniform int u_cascadeCount = 1;
    uniform float u_frustumSplits[MAX_CASCADES];
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
#endif

#if defined (CONTOUR)
    uniform float u_transparency;
    uniform float u_minHeight = 0.0;
    uniform float u_maxHeight = 1.0;
#endif

#if defined (COMP_SHADE)
    uniform vec4 u_colour = vec4(1.0);
#endif
//uniform float u_rim = 65.0;
//uniform float u_step = 0.8;
//uniform float u_mm = 1.0;

#if defined (NORMAL_MAP)
    uniform sampler2D u_normalMap;
    VARYING_IN vec2 v_normalTexCoord;
#endif

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_colour;
    VARYING_IN float v_ditherAmount;
    VARYING_IN vec3 v_cameraWorldPosition;
    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec2 v_texCoord;

    OUTPUT

#if defined(RX_SHADOWS)
    VARYING_IN LOW vec4 v_lightWorldPosition[MAX_CASCADES];
    VARYING_IN float v_viewDepth;

    int getCascadeIndex()
    {
        for(int i = 0; i < u_cascadeCount; ++i)
        {
            if (v_viewDepth >= u_frustumSplits[i])
            {
                return min(u_cascadeCount - 1, i);
            }
        }
        return u_cascadeCount - 1;
    }

    const float Bias = 0.001; //0.005
    float shadowAmount(int cascadeIndex)
    {
        vec4 lightWorldPos = v_lightWorldPosition[cascadeIndex];

        vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
        projectionCoords = projectionCoords * 0.5 + 0.5;
        float depthSample = TEXTURE(u_shadowMap, vec3(projectionCoords.xy, cascadeIndex)).r;
        float currDepth = projectionCoords.z - Bias;

        if (currDepth > 1.0)
        {
            return 1.0;
        }

        return (currDepth < depthSample) ? 1.0 : 1.0 - (0.3);
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
    float shadowAmountSoft(int cascadeIndex)
    {
        vec4 lightWorldPos = v_lightWorldPosition[cascadeIndex];

        vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
        projectionCoords = projectionCoords * 0.5 + 0.5;

        if(projectionCoords.z > 1.0) return 1.0;

        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
        for(int x = 0; x < filterSize; ++x)
        {
            for(int y = 0; y < filterSize; ++y)
            {
                float pcfDepth = TEXTURE(u_shadowMap, vec3(projectionCoords.xy + kernel[y * filterSize + x] * texelSize, cascadeIndex)).r;
                shadow += (projectionCoords.z - 0.001) > pcfDepth ? 0.4 : 0.0;
            }
        }

        float amount = shadow / 9.0;
        return 1.0 - amount;
    }
#endif

#if defined (ADD_NOISE)
    uniform vec4 u_noiseColour = vec4(0.0,0.0,0.0,1.0);

    const float NoisePerMetre = 10.0;
#include RANDOM
#endif

#include BAYER_MATRIX

#include HSV
    vec3 complementaryColour(vec3 c)
    {
        vec3 a = rgb2hsv(c);
        a.x += 0.25;
        a.z *= 0.5;
        c = hsv2rgb(a);
        return c;
    }

    const float Quantise = 10.0;
    const float TerrainLevel = -0.049;
    const float WaterLevel = -0.019;

    const vec3 SlopeShade = vec3(0.439, 0.368, 0.223);

    void main()
    {
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

#if !defined(COLOUR_LEVELS)
#define COLOUR_LEVELS 2.0
#define AMOUNT_MIN 0.8
#define AMOUNT_MAX 0.2
#else
#define AMOUNT_MIN 0.8
#define AMOUNT_MAX 0.2
#endif

        amount *= COLOUR_LEVELS;
        amount = round(amount);
        amount /= COLOUR_LEVELS;
        amount = AMOUNT_MIN + (amount * AMOUNT_MAX);

#if defined(COMP_SHADE)

        float tilt  = dot(normal, vec3(0.0, 1.0, 0.0));
        //tilt = ((smoothstep(0.97, 0.999, tilt) * 0.2)) * (1.0 - u_colour.r);
        tilt = ((1.0 - smoothstep(0.97, 0.999, tilt)) * 0.2) * (1.0 - u_colour.r);

//tilt *= 20.0;
//tilt = round(tilt);
//tilt /= 20.0;

        colour.rgb = mix(colour.rgb, colour.rgb * SlopeShade, tilt);
#else
        colour.rgb *= amount;
#endif

#if defined(TERRAIN)
        vec2 texCheck = v_texCoord * 4096.0;
        int texX = int(mod(texCheck.x, MatrixSize));
        int texY = int(mod(texCheck.y, MatrixSize));

        float facing = dot(normal, vec3(0.0, 1.0, 0.0));
        float waterFade = (1.0 - smoothstep(WaterLevel, WaterLevel + (1.15 * (1.0 - smoothstep(0.89, 0.99, facing))), v_worldPosition.y));
        float waterDither = findClosest(texX, texY, waterFade) * waterFade * (1.0 - step(0.96, facing));

#if defined(COMP_SHADE)
        waterDither *= u_colour.g;
#endif

        colour.rgb = mix(colour.rgb, colour.rgb * SlopeShade, waterDither * 0.5 * step(WaterLevel, v_worldPosition.y));
#endif

#define NOCHEX
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
        int cascadeIndex = getCascadeIndex();
        float shadow = shadowAmount(cascadeIndex);
        //float shadow = shadowAmountSmooth(cascadeIndex);
        float borderMix = smoothstep(u_frustumSplits[cascadeIndex] + 0.5, u_frustumSplits[cascadeIndex], v_viewDepth);
        if (borderMix > 0)
        {
            int nextIndex = min(cascadeIndex + 1, u_cascadeCount - 1);
            shadow = mix(shadow, shadowAmount(nextIndex), borderMix);
            //shadow = mix(shadow, shadowAmountSmooth(nextIndex), borderMix);
        }

        FRAG_OUT.rgb *= shadow;

//shows cascade boundries
//vec3 Colours[MAX_CASCADES] = vec3[MAX_CASCADES](vec3(0.2,0.0,0.0), vec3(0.0,0.2,0.0),vec3(0.0,0.0,0.2));
//for(int i = 0; i < u_cascadeCount; ++i)
//{
//    if (v_lightWorldPosition[i].w > 0.0)
//    {
//        vec2 coords = v_lightWorldPosition[i].xy / v_lightWorldPosition[i].w / 2.0 + 0.5;
//        if (coords.x > 0 && coords.x < 1 
//                && coords.y > 0 && coords.y < 1)
//        {
//            FRAG_OUT.rgb += Colours[i];
//        }
//    }
//}
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


#if defined (DITHERED) || defined (FADE_INPUT)// || defined (TERRAIN)
        vec2 xy = gl_FragCoord.xy;// / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

#if defined (FADE_INPUT)
        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, u_fadeAmount));
#else
        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
#endif
        if(alpha < 0.1) discard;
#endif

#if defined(CONTOUR)
    float height = min((v_worldPosition.y - u_minHeight) / (u_maxHeight - u_minHeight), 1.0);
    vec3 contourColour = mix(vec3(0.0,1.0,0.0), vec3(0.0,0.0,1.0), height * 2.0);
    //contourColour = mix(contourColour, vec3(1.0,0.0,1.0), max(0.0, height - 0.5) * 2.0);


//vec3 contourColour = vec3(1.0 - pow(1.0 - (0.1 + (height * 0.9)), 30.0), pow(1.0 - height, 5.0), 1.0 - pow(1.0 - (0.03 + (height * 0.97)), 40.0));


    vec3 f = fract(v_worldPosition * 2.0);
    vec3 df = fwidth(v_worldPosition * 2.0);
    vec3 g = step(df * u_pixelScale, f);
    //////vec3 g = smoothstep(df * 1.0, df * 2.0, f);
    ////float contour = 1.0 - (g.x * g.y * g.z);

    //vec3 faceNormal = normalize(cross(dFdx(v_worldPosition), dFdy(v_worldPosition))) * 20.0;

    float contourX = 1.0 - (g.y * g.z);

    //const float frequency = 40.0;
    //float dashX = (sin((mod(v_worldPosition.x, 1.0) * frequency) - (u_windData.w * faceNormal.x)) + 1.0) * 0.5;
    //contourX *= step(0.25, dashX);

    float contourZ = 1.0 - (g.y * g.x);
    //float dashZ = (sin((mod(v_worldPosition.z + 0.5, 1.0) * frequency) - (u_windData.w * faceNormal.z)) + 1.0) * 0.5;
    //contourZ *= step(0.25, dashZ);

    vec3 distance = v_worldPosition.xyz - v_cameraWorldPosition;
    float fade = (1.0 - smoothstep(36.0, 81.0, dot(distance, distance))) * u_transparency * 0.75;


    FRAG_OUT.rgb = mix(FRAG_OUT.rgb, contourColour, contourX * fade);
    FRAG_OUT.rgb = mix(FRAG_OUT.rgb, contourColour, contourZ * fade);
    FRAG_OUT.rgb += clamp(height, 0.0, 1.0) * 0.1;
#endif
#if defined(TERRAIN_CLIP)
    FRAG_OUT.rgb = mix(vec3(0.2, 0.3059, 0.6118), FRAG_OUT.rgb, smoothstep(WaterLevel - 0.001, WaterLevel + 0.001, v_worldPosition.y));

//if(v_worldPosition.y < WaterLevel) discard;//don't do this, it reveals the hidden trees.
#endif

//#if defined(COMP_SHADE) || defined(CONTOUR)
#if defined(BUNS)
        float rim = 1.0 - dot(normal, viewDirection);
        rim = smoothstep(u_step, 1.0, rim);
        rim = pow(rim, u_rim);
        
        //FRAG_OUT.rgb += vec3(rim * 0.05);
        FRAG_OUT.rgb = mix(FRAG_OUT.rgb, FRAG_OUT.rgb * u_mm, rim);

#endif
    })";