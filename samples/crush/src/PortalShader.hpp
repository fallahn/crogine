/*-----------------------------------------------------------------------

Matt Marchant 2021
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

static const std::string PortalVertex = 
R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldViewMatrix;
    uniform mat4 u_projectionMatrix;

    uniform float u_time;

    VARYING_OUT vec2 v_texCoord0;

    void main()
    {
        gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;

        v_texCoord0 = a_texCoord0;
        v_texCoord0.y -= u_time;
    })";

static const std::string PortalFragment =
R"(
    uniform sampler2D u_diffuseMap;

    VARYING_IN vec2 v_texCoord0;

    OUTPUT

    void main()
    {
        FRAG_OUT = TEXTURE(u_diffuseMap, v_texCoord0);
    })";