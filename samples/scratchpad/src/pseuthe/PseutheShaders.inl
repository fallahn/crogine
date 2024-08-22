#pragma once

#include <string>

static inline const std::string RayVertex =
R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;

ATTRIBUTE vec2 a_position;
ATTRIBUTE LOW vec4 a_colour;

VARYING_OUT LOW vec4 v_colour;
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

    FRAG_OUT.rgb = v_colour.rgb * v_colour.a * positionAlpha * falloffAlpha;
    FRAG_OUT.a = 1.0; //using additive blending
}
)";
