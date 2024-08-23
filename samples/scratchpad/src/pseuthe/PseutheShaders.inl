#pragma once

#include <string>

static inline const std::string RayVertex =
R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;

ATTRIBUTE vec2 a_position;
ATTRIBUTE vec4 a_colour;

VARYING_OUT vec4 v_colour;
VARYING_OUT vec2 v_position;

void main()
{
    gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
    v_colour = a_colour;
    v_position = a_position;
}
)";

static inline const std::string RayFragment =
R"(
uniform float u_alpha = 1.0; //normalised X dist * PI

VARYING_IN vec4 v_colour;
VARYING_IN vec2 v_position;

OUTPUT

const float FalloffStart = 800.0;
const float FalloffDistance = 700.0;

void main()
{
    float falloffAlpha = 1.0;
    float rayLen = length(v_position);
    float amount = step(FalloffStart, rayLen);

    //x screen position
    float positionAlpha = 1.0 - pow(cos(u_alpha), 4.0);

    //ray length falloff
    falloffAlpha -= clamp((rayLen - FalloffStart) / FalloffDistance, 0.0, 1.0) * amount;

    FRAG_OUT.rgb = v_colour.rgb * v_colour.a * positionAlpha;// * falloffAlpha;
    FRAG_OUT.a = 1.0; //using additive blending
}
)";

static inline const std::string OrbVertex =
R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;
uniform mat3 u_normalMatrix;

uniform vec3 u_lightPosition = vec3(960.0, -10.0, 10.0);

ATTRIBUTE vec2 a_position;
ATTRIBUTE vec2 a_texCoord0;
ATTRIBUTE vec4 a_colour;

VARYING_OUT vec3 v_lightDirection;
VARYING_OUT vec4 v_colour;
VARYING_OUT vec2 v_texCoord;

const vec3 tangent = vec3(1.0, 0.0, 0.0);
const vec3 normal = vec3(0.0, 0.0, 1.0);

void main()
{
    vec4 worldVertex = u_worldMatrix * vec4(a_position, 0.0, 1.0);
    gl_Position = u_viewProjectionMatrix * worldVertex;
    v_colour = a_colour;
    v_texCoord = a_texCoord0;

    vec3 n = normalize(u_normalMatrix * normal);
    vec3 t = normalize(u_normalMatrix * tangent);
    vec3 b = cross(n, t);

    vec3 worldLightDirection = u_lightPosition - worldVertex.xyz;
    v_lightDirection.x = dot(worldLightDirection, t);
    v_lightDirection.y = dot(worldLightDirection, b);
    v_lightDirection.z = dot(worldLightDirection, n);

    //v_eyeDirection.x = dot(-worldVertex, t);
    //v_eyeDirection.y = dot(-worldVertex, b);
    //v_eyeDirection.z = dot(-worldVertex, n);
})";

static inline const std::string OrbFragment =
R"(
OUTPUT
#line 1
uniform sampler2D u_texture;
uniform sampler2D u_normalMap;
uniform float u_lightIntensity = 1.0;

//TODO make ambient colour match background
uniform vec3 u_ambientColour = vec3 (0.1, 0.1, 0.1);

VARYING_IN vec3 v_lightDirection;
VARYING_IN vec4 v_colour;
VARYING_IN vec2 v_texCoord;

const vec3 lightColour = vec3(1.0, 0.98, 0.745);
const float inverseRange = 0.005;

vec2 UVSize = vec2(1.0/8.0, 1.0/4.0);

void main()
{
    vec4 diffuseColour = TEXTURE(u_texture, v_texCoord);
    vec3 normalVector = normalize(TEXTURE(u_normalMap, v_texCoord).rgb * 2.0 - 1.0);

    vec3 blendedColour = diffuseColour.rgb * u_ambientColour;
    float diffuseAmount = max(dot(normalVector, normalize(v_lightDirection)), 0.0);

    //multiply by falloff
    vec3 falloffDirection = v_lightDirection * inverseRange;
    float falloff = clamp(1.0 - dot(falloffDirection, falloffDirection), 0.0, 1.0);
    
    float lightIntensity = 1.0 - pow(cos(u_lightIntensity), 4.0);
    blendedColour += (lightColour * lightIntensity) * diffuseColour.rgb * diffuseAmount;

    FRAG_OUT.rgb = blendedColour * v_colour.rgb;
    FRAG_OUT.a = diffuseColour.a; //this is moot we're using additive blending


    //outer circle
    vec3 ringColour = v_colour.rgb * 0.5;
    ringColour *= u_ambientColour;
    ringColour += (lightColour * lightIntensity) * v_colour.rgb;

    vec2 UVNorm = mod(v_texCoord, UVSize) / UVSize;
    float l = length(UVNorm - vec2(0.5));
    float circleInner = smoothstep(0.46, 0.482, l);
    
    blendedColour += vec3(v_colour.r * (100.0 / 255.0)) * (1.0 - circleInner);
    FRAG_OUT.rgb = mix(blendedColour, ringColour, circleInner * (1.0 - smoothstep(0.498, 0.5, l)));

})";