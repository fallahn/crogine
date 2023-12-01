
#pragma once

#include <string>

inline const std::string ProjectionVertex =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;

uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;
uniform mat4 u_targetViewProjectionMatrix;

VARYING_OUT vec4 v_targetPosition;

void main()
{
    vec4 worldPos = u_worldMatrix * a_position;
    v_targetPosition = u_targetViewProjectionMatrix * worldPos;
    gl_Position = u_viewProjectionMatrix * worldPos;
}
)";

inline const std::string ProjectionFragment =
R"(
OUTPUT

VARYING_IN vec4 v_targetPosition;

const float RingCount = 5.0;

void main()
{
    vec2 coord = v_targetPosition.xy/v_targetPosition.w;

    float l = length(coord);
    float r = step(0.0, sin(min(RingCount, l * RingCount) * 3.14));
    vec3 target = mix(vec3(1.0), vec3(1.0, 0.0, 0.0), r);
    vec3 base = vec3(0.0, 1.0, 1.0);

    FRAG_OUT = vec4(mix(target, base, step(1.0, l)), 1.0);
})";
