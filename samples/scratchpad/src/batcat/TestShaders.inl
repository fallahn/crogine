#pragma once

#include <string>

static const inline std::string HoloFrag =
R"(
OUTPUT
#include CAMERA_UBO


uniform sampler2D u_diffuseMap;
uniform vec4 u_colour;

uniform float u_time;
#line 1

VARYING_IN vec2 v_texCoord0;
VARYING_IN vec3 v_normalVector;
VARYING_IN vec3 v_worldPosition;

const float LineFreq = 1000.0;
const float FarAmount = 0.3;
const float NearAmount = 0.6;

void main()
{
    vec2 uv = v_texCoord0.xy;
    uv.x += u_time * 0.02;

    float offset = (sin((u_time * 0.5) + (uv.y * 10.0)));
    uv.x += step(0.6, offset) * offset * 0.01;

    vec4 tint = mix(u_colour * FarAmount, u_colour * NearAmount, 
                    step(0.5, (dot(normalize(u_cameraWorldPosition - v_worldPosition), normalize(v_normalVector)) + 1.0 * 0.5)));

    vec4 colour = TEXTURE(u_diffuseMap, uv) * tint * 0.94;

    float scanline = ((sin((uv.y * LineFreq) + (u_time * 30.3)) + 1.0 * 0.5) * 0.4) + 0.6;
    colour *= mix(1.0, scanline, step(0.58, uv.y)); //arbitrary cut off for texture
    //flicker
    colour *= (step(0.95, fract(sin(u_time * 0.01) * 43758.5453123)) * 0.2) + 0.8;

    FRAG_OUT = colour;
})";

static const inline std::string LavaFrag =
R"(
OUTPUT
#include CAMERA_UBO

uniform float u_time;
#line 1

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

//const vec3 baseColour = vec3(0.949, 0.812, 0.361);
const vec3 baseColour = vec3(0.925, 0.467, 0.239);
const vec3 LumConst = vec3(0.2125, 0.7154, 0.0721);
const float Scale = 2.0;
const float Speed = 1.0 / 30.0;

void main()
{
    vec2 uv = v_texCoord0 / Scale;

    uv.y += (cos(u_time * Speed) * 0.1) + (u_time * Speed);
    uv.x *= sin(u_time + uv.y * 4.0) * 0.1 + 0.8;
    uv += noise((uv * 2.25) + (u_time / 5.0));

    vec3 darkColour = baseColour;
    darkColour.rg += 0.3 * sin((uv.y * 4.0) + u_time) * sin((uv.x * 5.0) + u_time);

    float amt = smoothstep(0.01, 0.3, noise(uv * 3.0))
        + smoothstep(0.01, 0.3, noise(uv * 6.0 + 0.5))
        + smoothstep(0.01, 0.4, noise(uv * 7.0 + 0.2));


    vec3 finalColour = mix(baseColour, darkColour, vec3(smoothstep(0.0, 1.0, amt)));
    float luminance = clamp(dot(finalColour, LumConst) * 1.5, 0.0, 1.0);

    //FRAG_OUT.rgb = vec3(luminance);
    FRAG_OUT.rgb = finalColour;
    FRAG_OUT.a = 1.0;

})";

static const inline std::string LavaFragV2 =
R"(
OUTPUT

#include CAMERA_UBO

uniform float u_time;

VARYING_IN vec2 v_texCoord0;

const float Speed = 0.2;
const float Scale = 3.0;
const int Iterations = 3;

const mat3 Rotation = mat3(-2.0, -1.0, 2.0,
                            3.0, -2.0, 1.0,
                            1.0,  2.0, 2.0);
const float Density = 0.3;
const float Intensity = 25.0;
const float Brightness = 0.9;

const vec4 Dark = vec4(1.0, 0.3608, 0.098, 1.0);
const vec4 Light = vec4(1.0, 0.6275, 0.1725, 1.0);

void main()
{
    float time = u_time * Speed;

    vec4 caustic = vec4(vec3(v_texCoord0 * Scale, 0.0), time);
    caustic.xy += sin(time);

    float accum = 1.0;
    for (int i = 0; i < Iterations; ++i)
    {
        accum = min(accum, length(0.5 - fract(caustic.xyw *= Rotation * Density)));
    }

    float amt = pow(accum, 7.0) * Intensity;
    
    //FRAG_OUT = vec4(clamp(amt + Brightness, 0.0 ,1.0));
    FRAG_OUT = mix(Dark, Light, amt);
})";