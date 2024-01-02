/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

inline const std::string BushVertex =
R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;

#if defined(INSTANCING)
#include INSTANCE_ATTRIBS
#else
    uniform mat3 u_normalMatrix;
#endif
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat4 u_projectionMatrix;
    uniform vec3 u_cameraWorldPosition;

    uniform vec4 u_clipPlane;
    uniform float u_targetHeight = 300.0; //height of the render target multiplied by its viewport, ie height of the renderable area

    uniform float u_leafSize = 0.25; //world units, in this case metres
    uniform float u_randAmount = 0.2;

    uniform sampler2D u_noiseTexture;

//dirX, strength, dirZ, elapsedTime
#include WIND_BUFFER

#include RESOLUTION_BUFFER

    out vData
    {
        mat2 rotation;
        vec4 colour;
        vec3 normal;
        vec3 worldPos;
        float darkenAmount;
        float ditherAmount;
    }v_data;

#include RANDOM
#include WIND_CALC

    const float MaxWindOffset = 0.1f;
    const float Amp = 0.01;

const float MinVisibility = 0.3;
const float MinWindDistance = 50.0;

    void main()
    {
        //int UID = gl_InstanceID << 16 | (gl_VertexID & 0x0000ffff);
        //int UID = gl_InstanceID + gl_VertexID;

    #if defined (INSTANCING)
#include INSTANCE_MATRICES
    #else
        mat4 worldMatrix = u_worldMatrix;
        mat3 normalMatrix = u_normalMatrix;
    #endif

vec4 translation = worldMatrix[3];

        float randVal = rand(vec2(translation.x + gl_VertexID, translation.z + gl_VertexID));
        float offset = randVal * u_randAmount;
        vec4 position = a_position;
    #if defined(HQ)
        position.xyz = (a_normal * offset) + position.xyz;
    #endif

        v_data.normal = normalMatrix * a_normal;
        v_data.colour = a_colour * (1.0 - (u_randAmount - offset)); //darken less offset leaves

    #if !defined(HQ)
        float t = (u_windData.w * 15.0) + gl_InstanceID + gl_VertexID;
        float highFreq = sin(t) * Amp * a_colour.r;
        position.y = (highFreq * ((0.8 * u_windData.y) + 0.2)) + position.y;
    #endif

        vec4 worldPosition = worldMatrix * position;


        vec3 camForward = vec3(u_viewMatrix[0][2], u_viewMatrix[1][2], u_viewMatrix[2][2]);
        vec3 eyeDir = normalize(u_cameraWorldPosition - worldPosition.xyz);
        float visibility = dot(eyeDir, (camForward));


//wind
float distance = length(worldPosition.xyz - u_cameraWorldPosition);
    #if defined (HQ)
if(visibility > MinVisibility
    && distance < MinWindDistance){
        WindResult windResult = getWindData(position.xz, worldPosition.xz);
        windResult.lowFreq *= (0.5 * u_windData.y) + 0.5;
        windResult.highFreq *= (0.5 * u_windData.y) + 0.5;

        float x = windResult.highFreq.x;
        float y = windResult.lowFreq.y;
        vec3 windOffset = vec3(x, y, windResult.highFreq.y) * 5.0;

        vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
        float dirStrength = dot(v_data.normal, windDir);

        x *= 3.0;
        vec2 rot = vec2(sin(x * u_windData.y), cos(x * u_windData.y));
        v_data.rotation[0] = vec2(rot.y, -rot.x);
        v_data.rotation[1]= rot;

        dirStrength += 1.0;
        dirStrength *= 0.5;


        windOffset += windDir * u_windData.y * dirStrength * 2.0;
        worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;

        worldPosition.x += windResult.lowFreq.x;
        worldPosition.z += windResult.lowFreq.y;
}else{
        v_data.rotation = mat2(1.0);
}
    #else
        float time = (u_windData.w * 15.0) + gl_InstanceID;
        float x = sin(time * 2.0) * 0.125;
        float y = cos(time) * 0.5;
        vec3 windOffset = vec3(x, y, x) * a_colour.b * 0.1;


        vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
        float dirStrength = a_colour.b;

        windOffset = (windDir * u_windData.y * dirStrength) + windOffset;
        worldPosition.xyz = (windOffset * MaxWindOffset * u_windData.y) + worldPosition.xyz;
    #endif

#if defined(WOBBLE)
        vec4 vertPos = u_viewProjectionMatrix * worldPosition;
        vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;

        gl_Position = vertPos;
#else
        gl_Position = u_viewProjectionMatrix * worldPosition;
#endif


//size calc

    #if defined(HQ)
        //distance = length(worldPosition.xyz - u_cameraWorldPosition);

if(visibility > MinVisibility){
        float variation = rand(-vec2(gl_VertexID));
        variation = (0.5 * variation) + 0.5;

        float pointSize = ((u_leafSize * 2.0) * offset) + u_leafSize;
        pointSize *= variation;
            
        float facingAmount = dot(v_data.normal, camForward);
        pointSize *= (0.2 * facingAmount) + 0.8;
            
        //shrink 'backfacing' to zero
        //pointSize *= step(0.0, facingAmount);
        pointSize *= smoothstep(-0.2, 0.0, facingAmount);
            
        //we use the camera's forward vector to shrink any points out of view to zero
        pointSize *= step(0.0, clamp(visibility, 0.0, 1.0));

        //shrink with perspective/distance and scale to world units
        pointSize *= u_targetHeight * (u_projectionMatrix[1][1] / gl_Position.w);

        //we scale point size by model matrix but it assumes all axis are
        //scaled equally, as we only use the X axis
        pointSize *= length(worldMatrix[0].xyz);

        //after X distance set every other vert size to 0
        //which will cause the geom shader to cull it
        pointSize *= 1.0 - (smoothstep(0.5, 1.0, distance / 150.0) * (gl_VertexID & 1));

        gl_PointSize = pointSize;
}else{ gl_PointSize = 0.0; }
    #else
        //float distance = length(worldPosition.xyz - u_cameraWorldPosition);
    #endif

//proximity fade
        float fadeDistance = u_nearFadeDistance * 5.0;//2.0; //I forget what this magic number was for. Lesson learned?
        const float farFadeDistance = 360.f;

        v_data.ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
        //fades far leaves before culling kicks in at 280m
        v_data.ditherAmount *= 1.0 - clamp((distance - 255.0) / 25.0, 0.0, 1.0);

        v_data.darkenAmount = (((1.0 - pow(clamp(distance / farFadeDistance, 0.0, 1.0), 5.0)) * 0.8) + 0.2);

        gl_ClipDistance[0] = dot(worldPosition, u_clipPlane);

        v_data.worldPos = worldPosition.xyz;
    })";

inline const std::string BushGeom = R"(

#if defined (POINTS)
    layout (points) in;
    layout (points, max_vertices = 1) out;
//hax because mac gpu drivers suck
#define ARRAY_SIZE 1
#else
    layout (triangles) in;
    layout (triangles, max_vertices = 3) out;
#define ARRAY_SIZE 3
#endif

    in vData
    {
        mat2 rotation;
        vec4 colour;
        vec3 normal;
        vec3 worldPos;
        float darkenAmount;
        float ditherAmount;
    }v_dataIn[ARRAY_SIZE];


    out vData
    {
        mat2 rotation;
        vec4 colour;
        vec3 normal;
        vec3 worldPos;
        float darkenAmount;
        float ditherAmount;
    } v_dataOut;

    void main()
    {
#if defined (POINTS)

        gl_Position = gl_in[0].gl_Position;
        gl_PointSize = gl_in[0].gl_PointSize;
        gl_ClipDistance[0] = gl_in[0].gl_ClipDistance[0];
    
        v_dataOut.rotation = v_dataIn[0].rotation;
        v_dataOut.colour = v_dataIn[0].colour;
        v_dataOut.normal = v_dataIn[0].normal;
        v_dataOut.darkenAmount = v_dataIn[0].darkenAmount;
        v_dataOut.ditherAmount = v_dataIn[0].ditherAmount;

        if(gl_PointSize > 0.05)
        {
            EmitVertex();
        }

#else

        for(int i = 0; i < gl_in.length(); ++i)
        {
            gl_Position = gl_in[i].gl_Position;
            gl_PointSize = gl_in[i].gl_PointSize;
            gl_ClipDistance[0] = gl_in[i].gl_ClipDistance[0];
        
            v_dataOut.rotation = v_dataIn[i].rotation;
            v_dataOut.colour = v_dataIn[i].colour;
            v_dataOut.normal = v_dataIn[i].normal;
            v_dataOut.darkenAmount = v_dataIn[i].darkenAmount;
            v_dataOut.ditherAmount = v_dataIn[i].ditherAmount;

            if(gl_PointSize > 0.05)
            {
                EmitVertex();
            }
        }
#endif
    })";

inline const std::string BushFragment =
R"(
#include OUTPUT_LOCATION

    uniform sampler2D u_diffuseMap;
    uniform vec3 u_lightDirection;
    uniform vec3 u_colour = vec3(1.0);
    uniform vec4 u_lightColour;

#include SCALE_BUFFER

    in vData
    {
        mat2 rotation;
        vec4 colour;
        vec3 normal;
        vec3 worldPos;
        float darkenAmount;
        float ditherAmount;
    }v_data;

#include BAYER_MATRIX
#include LIGHT_COLOUR

#include HSV
    vec3 complementaryColour(vec3 c)
    {
        vec3 a = rgb2hsv(c);
        a.x += 0.25;
        a.z *= 0.5;
        c = hsv2rgb(a);
        return c;
    }

    void main()
    {
        vec3 normal = normalize(v_data.normal);
        float amount = dot(normal, -u_lightDirection);
        amount *= 2.0;
        amount = round(amount);
        amount *= 0.5;
        amount = (amount * 0.6) + 0.4;
#if defined(VERTEX_COLOURED)
        vec3 colour = mix(complementaryColour(v_data.colour.rgb), v_data.colour.rgb, amount);
#else
        vec3 colour = mix(complementaryColour(u_colour.rgb), u_colour.rgb, amount);
        //multiply by v_data.colour.a to darken on leaf depth - looks nice but not used
#endif

        vec2 coord = gl_PointCoord.xy;
        coord = v_data.rotation * (coord - vec2(0.5));
        coord += vec2(0.5);

//use texture and dither amount to see if we discard
#if defined(HQ)
        vec4 textureColour = TEXTURE(u_diffuseMap, coord);
#else
        vec4 textureColour = vec4(1.0);
#endif
        vec2 xy = gl_FragCoord.xy;// / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_data.ditherAmount));
        if (textureColour.a * alpha < 0.3) discard;

        textureColour.rgb *= v_data.darkenAmount;

#if defined(USE_MRT)
        NORM_OUT = vec4(normal, 1.0);
        POS_OUT = vec4(v_data.worldPos, 1.0);
        LIGHT_OUT = vec4(vec3(0.0), 1.0);
#endif

        FRAG_OUT = vec4(colour, 1.0) * textureColour * getLightColour();
    })";

inline const std::string BranchVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec2 a_texCoord0;

#if defined(INSTANCING)
#include INSTANCE_ATTRIBS
#else
    uniform mat3 u_normalMatrix;
#endif

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform vec4 u_clipPlane;
    uniform vec3 u_cameraWorldPosition;

    uniform sampler2D u_noiseTexture;

//dirX, strength, dirZ, elapsedTime
#include WIND_BUFFER

#include RESOLUTION_BUFFER

    VARYING_OUT float v_ditherAmount;
    VARYING_OUT vec2 v_texCoord;
    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT float v_darkenAmount;

#include WIND_CALC

    void main()
    {
    #if defined (INSTANCING)
        mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
        mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;
    #else
        mat4 worldMatrix = u_worldMatrix;
        mat3 normalMatrix = u_normalMatrix;
    #endif

        vec4 position = a_position;
        WindResult windResult = getWindData(position.xz, worldMatrix[3].xz);

        vec3 vertexStrength = a_colour.rgb;
        //multiply high and low frequency by vertex colours
        //note that in tree models red/green ARE SWAPPED >.<
        windResult.highFreq *= vertexStrength.r;
        windResult.lowFreq *= vertexStrength.g;

        //apply high frequency and low frequency in local space
        position.x += windResult.lowFreq.x + windResult.highFreq.x;
        position.z += windResult.lowFreq.y + windResult.highFreq.y;

        //multiply wind direction by wind strength
        vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * vertexStrength.b;
        //wind dir is added in world space (below)

        vec4 worldPosition = worldMatrix * position;
        worldPosition.xyz += windDir;


#if defined(WOBBLE)
        vec4 vertPos = u_viewProjectionMatrix * worldPosition;
        vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;

        gl_Position = vertPos;
#else
        gl_Position = u_viewProjectionMatrix * worldPosition;
#endif
        v_worldPosition = worldPosition.xyz;

        gl_ClipDistance[0] = dot(u_clipPlane, worldPosition);

        v_texCoord = a_texCoord0;
        v_normal = normalMatrix * a_normal;

//proximity fade
        float fadeDistance = u_nearFadeDistance * 5.0;//2.0;
        const float farFadeDistance = 360.f;
        float distance = length(worldPosition.xyz - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
        //fades far leaves before culling kicks in at 280m
        v_ditherAmount *= 1.0 - clamp((distance - 255.0) / 25.0, 0.0, 1.0);

        v_darkenAmount = (((1.0 - pow(clamp(distance / farFadeDistance, 0.0, 1.0), 5.0)) * 0.8) + 0.2);
    })";

inline const std::string BranchFragment = R"(
#include OUTPUT_LOCATION

    uniform sampler2D u_diffuseMap;
    uniform vec3 u_lightDirection;
    uniform vec4 u_lightColour;

#include SCALE_BUFFER

    VARYING_IN float v_ditherAmount;
    VARYING_IN vec2 v_texCoord;
    VARYING_IN vec3 v_normal;
    VARYING_IN vec3 v_worldPosition;
    VARYING_IN float v_darkenAmount;

#include BAYER_MATRIX
#include LIGHT_COLOUR

    void main()
    {
        vec4 colour = TEXTURE(u_diffuseMap, v_texCoord);

        vec3 normal = normalize(v_normal);
        float amount = dot(normal, -u_lightDirection);
        amount *= 2.0;
        amount = round(amount);
        amount *= 0.5;
        amount = (amount * 0.4) + 0.6;

        colour.rgb *= amount * v_darkenAmount;
        FRAG_OUT = colour * getLightColour();

#if defined(USE_MRT)
        NORM_OUT = vec4(normal, 1.0);
        POS_OUT = vec4(v_worldPosition, 1.0);
        LIGHT_OUT = vec4(vec3(0.0), 1.0);
#endif


        vec2 xy = gl_FragCoord.xy;// / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
#if defined ALPHA_CLIP
        alpha *= colour.a;
#endif

        if (alpha < 0.1) discard;
    })";