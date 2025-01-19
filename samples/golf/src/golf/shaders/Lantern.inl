/*-----------------------------------------------------------------------

Matt Marchant 2024 - 2025
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

static const inline std::string RopeFrag =
R"(
uniform vec4 u_colour = vec4(0.6784, 0.7255, 0.7216, 1.0);
OUTPUT
void main(){FRAG_OUT = u_colour;}
)";

static const inline std::string LanternVert =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_colour;
ATTRIBUTE vec3 a_normal;

#include CAMERA_UBO
#include WVP_UNIFORMS

VARYING_OUT vec3 v_worldPosition;
VARYING_OUT vec4 v_colour;
VARYING_OUT vec3 v_normalVector;

void main()
{
    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
    gl_Position = wvp * a_position;

    v_worldPosition = (u_worldMatrix * a_position).xyz;
    v_colour = a_colour;

    v_normalVector = u_normalMatrix * a_normal;
}
)";

static const inline std::string LanternFrag =
R"(
#include CAMERA_UBO

uniform vec4 u_ballColour; //not a ball, but we need the name to match up (see createRopes() in MainMenu)

VARYING_IN vec3 v_worldPosition;
VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_normalVector;

OUTPUT

const float RimStart = 0.5;
const float RimEnd = 0.99;
const float RimAttenuation = 0.4;

void main()
{
    vec3 normal = normalize(v_normalVector);
    vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
    float rim = dot(normal, eyeDirection);
    rim = smoothstep(RimStart, RimEnd, rim) * RimAttenuation;

    FRAG_OUT = (v_colour * u_ballColour) + vec4(rim);
    FRAG_OUT.a = 1.0;
})";