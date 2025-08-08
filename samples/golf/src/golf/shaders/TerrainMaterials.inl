#pragma once

#include <string>

//u_windData.w == elapsed time
static const inline std::string LavaFrag =
R"(

#define USE_MRT //for output location
#include CAMERA_UBO
#include WIND_BUFFER
#include OUTPUT_LOCATION

VARYING_IN vec2 v_texCoord0;

vec2 randV2(vec2 coord)
{
    coord = vec2(dot(vec2(127.1,311.7), coord), dot(vec2(269.5,183.3), coord));
    return (2.0 * fract(sin(coord) * 43758.5453123)) - 1.0;
}


//value Noise by Inigo Quilez - iq/2013 (MIT)
//https://www.shadertoy.com/view/lsf3WH
float noise(vec2 st)
{
    vec2 i = floor(st);
    vec2 f = fract(st);

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(dot(randV2(i + vec2(0.0,0.0)), f - vec2(0.0,0.0)), 
                     dot(randV2(i + vec2(1.0,0.0)), f - vec2(1.0,0.0)), u.x),
                mix(dot(randV2(i + vec2(0.0,1.0)), f - vec2(0.0,1.0)), 
                     dot(randV2(i + vec2(1.0,1.0)), f - vec2(1.0,1.0)), u.x), u.y);
}

const vec3 baseColour = vec3(0.925, 0.467, 0.239);
const vec3 LumConst = vec3(0.2125, 0.7154, 0.0721);
const float Scale = 0.5;//2.0;
const float Speed = 1.0 / 30.0;

void main()
{
	vec2 uv = v_texCoord0 / Scale;
    float time = u_windData.w / 10.0;

    uv.y += (cos(time * Speed) * 0.1) + (time * Speed);
    uv.x *= sin(time + uv.y * 4.0) * 0.1 + 0.8;
    uv += noise((uv * 2.25) + (time / 5.0));

    vec3 darkColour = baseColour;
    darkColour.rg += 0.1 * sin((uv.y * 4.0) + time) * sin((uv.x * 5.0) + time);

    float amt = smoothstep(0.01, 0.3, noise(uv * 3.0))
        + smoothstep(0.01, 0.3, noise(uv * 6.0 + 0.5))
        + smoothstep(0.01, 0.4, noise(uv * 7.0 + 0.2));


    vec3 finalColour = mix(baseColour, darkColour, vec3(smoothstep(0.0, 1.0, amt)));
    float luminance = clamp(dot(finalColour, LumConst) * 1.5, 0.0, 1.0);

    LIGHT_OUT = vec4(finalColour, 1.0);
    NORM_OUT.a = 1.0 - luminance;

    FRAG_OUT.rgb = finalColour;
    FRAG_OUT.a = 1.0;

})";
