/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

inline const std::string WaterVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec3 a_normal;

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform mat4 u_reflectionMatrix;
    uniform mat4 u_lightViewProjectionMatrix;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec2 v_texCoord;

    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT vec4 v_reflectionPosition;
    VARYING_OUT vec4 v_refractionPosition;
    VARYING_OUT LOW vec4 v_lightWorldPosition;
    VARYING_OUT vec2 v_vertDistance;
    VARYING_OUT float v_scale;

    const vec2 MapSize = vec2(320.0, 200.0);

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * position;

        v_normal = u_normalMatrix * a_normal;

        v_texCoord = vec2(position.x / MapSize.x, -position.z / MapSize.y);
        v_texCoord += vec2(1.0); //remove negative value up to -1,-1

        v_worldPosition = position.xyz;
        v_reflectionPosition = u_reflectionMatrix * position;
        v_refractionPosition = gl_Position;
        v_lightWorldPosition = u_lightViewProjectionMatrix * position;

        v_vertDistance = a_position.xy;

        v_scale = u_worldMatrix[0][0];
    })";

inline const std::string WaterFragment = R"(
#include OUTPUT_LOCATION
    #define MAX_RADIUS 239.9

    uniform sampler2D u_reflectionMap;
#if defined(RAIN)
    uniform sampler2D u_rainTexture;
    uniform float u_rainAmount;
#endif

#if !defined(NO_DEPTH)
uniform sampler2DArray u_depthMap;
#endif
    uniform vec3 u_cameraWorldPosition;
    uniform float u_radius = MAX_RADIUS;

//dirX, strength, dirZ, elapsedTime
#include WIND_BUFFER

#include SCALE_BUFFER

    VARYING_IN vec3 v_normal;
    VARYING_IN vec2 v_texCoord;

    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec4 v_reflectionPosition;
    VARYING_IN vec4 v_refractionPosition;

    VARYING_IN vec2 v_vertDistance;
    VARYING_IN float v_scale;

    //pixels per metre
    const vec2 PixelCount = vec2(1280.0, 800.0);
    const vec2 TileCount = vec2(320.0, 200.0); //tiles of rain animation

    const vec3 WaterColour = vec3(0.02, 0.078, 0.578);
    //const vec3 WaterColour = vec3(0.2, 0.278, 0.278);
    //const vec3 WaterColour = vec3(0.137, 0.267, 0.53);

#include RANDOM

#include BAYER_MATRIX

    float linearise(float d)
    {
        const float zNear = 10.02; //note these have to match the near/far plane of the depthmap camera
        const float zFar = 10.48;

        return zNear * zFar / ((d * (zNear - zFar)) + zFar);
    }

#if !defined(NO_DEPTH)
    float getDepth()
    {
        const float ColCount = 8.0;
        const float MetresPerTexture = 40.0;

        float x = floor((v_worldPosition.x / MetresPerTexture));
        float y = floor((-v_worldPosition.z / MetresPerTexture));
        float index = clamp((y * ColCount) + x, 0.0, 39.0);

        float u = (v_worldPosition.x - (x * MetresPerTexture)) / MetresPerTexture;
        float v = -((y * MetresPerTexture) + v_worldPosition.z) / MetresPerTexture;

        float stepX = step(0.0, v_worldPosition.x) * (1.0 - step(320.0, v_worldPosition.x));
        float stepY = step(0.0, -v_worldPosition.z) * (1.0 - step(200.0, -v_worldPosition.z));

        return (1.0 - texture(u_depthMap, vec3(u, v, index)).r) * stepX * stepY;
    }
#endif

    void main()
    {
        //sparkle
        float waveSpeed = (u_windData.w * 7.5);
        vec2 coord = v_texCoord;
        //coord.y += waveSpeed / (PixelCount.y * 4.0); //makes the waves drift across the surface

        vec2 pixelCoord = floor(mod(coord, 1.0) * PixelCount);
        float wave = noise(pixelCoord);
        wave *= sin(waveSpeed + (wave * 300.0)) + 1.0 * 0.5;
        wave = smoothstep(0.25, 1.0, wave);
        

        float coordOffset = sin(((u_windData.w * 15.0) * 0.25) + (gl_FragCoord.z * 325.0)) * 0.0002;
        coordOffset += wave * 0.002;

        //reflection
        vec2 reflectCoords = v_reflectionPosition.xy / v_reflectionPosition.w / 2.0 + 0.5;
        reflectCoords.x += coordOffset;

        vec4 reflectColour = TEXTURE(u_reflectionMap, reflectCoords);

        vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
        vec3 normal = normalize(v_normal);

#if defined (USE_MRT)
        NORM_OUT = vec4(normal, 1.0);
        POS_OUT = vec4(v_worldPosition, 1.0);
        LIGHT_OUT = vec4(vec3(0.0), 1.0);
#endif


        float fresnel = dot(reflect(-eyeDirection, normal), normal);
        const float bias = 0.6;
        fresnel = (fresnel * (1.0 - bias)) + bias;

        vec3 blendedColour = mix(reflectColour.rgb, WaterColour.rgb, fresnel);

        float edgeWave = wave;

        //wave *= 0.2 * pow(reflectCoords.y, 4.0);
        wave *= (0.19 * pow(reflectCoords.y, 4.0)) + 0.01;

        blendedColour.rgb += wave;

        //edge feather
        float amount = 1.0 - smoothstep(u_radius * 0.625, u_radius, length(v_vertDistance));

        vec2 xy = gl_FragCoord.xy;// / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));
        float alpha = findClosest(x, y, amount);

        if(alpha < 0.1) discard;
#if !defined(NO_DEPTH)
        //wave at edge/intersection

        //xy = gl_FragCoord.xy / u_pixelScale;

        xy = vec2(v_worldPosition.x, -v_worldPosition.z) * 32.0;
        x = int(mod(xy.x, MatrixSize));
        y = int(mod(xy.y, MatrixSize));

        float depth = getDepth() * clamp(v_scale, 0.0, 1.0);
        depth *= findClosest(x,y,pow(depth, 3.0));
        
        edgeWave *= 0.07;
        blendedColour = (edgeWave * depth) + blendedColour;
        blendedColour = (depth * 0.18) + blendedColour;
#endif

#if defined(RAIN)
        blendedColour = (TEXTURE(u_rainTexture, v_texCoord /** TileCount*/).rgb * u_rainAmount) + (blendedColour);
#endif

        FRAG_OUT = vec4(blendedColour, 1.0);
    })";

    inline const std::string HorizonVert = 
        R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;

    void main()
    {
        gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;

        v_colour = a_colour;
        v_texCoord = a_texCoord0;
    })";

    inline const std::string HorizonFrag = 
        R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;
    uniform vec4 u_lightColour = vec4(1.0);

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    const vec3 WaterColour = vec3(0.02, 0.078, 0.578);

    void main()
    {
#if defined (SUNLIGHT)
        vec4 colour = TEXTURE(u_diffuseMap, v_texCoord) * u_lightColour;
#else
        vec4 colour = TEXTURE(u_diffuseMap, v_texCoord);
#endif
        FRAG_OUT = vec4(mix(WaterColour, colour.rgb, v_colour.g), 1.0);

        if(colour.a < 0.1) discard;
    })";