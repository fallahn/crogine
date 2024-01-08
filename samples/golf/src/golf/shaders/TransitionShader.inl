/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
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

inline const std::string ScanlineTransition =
R"(
uniform float u_time;
uniform vec2 u_scale;
uniform vec2 u_resolution;

//in vec2 v_texCoord;
in vec4 v_colour;
out vec4 FRAG_OUT;

const float MaxTime = 1.0;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution;
    float lineCount = u_resolution.y;

    float pass = mod(u_time, MaxTime * 2.0);
    pass = floor(pass / MaxTime);


    float t = mod(u_time, MaxTime);
    float progress = t / MaxTime;
       
    float y = 1.0 - floor(progress * lineCount) / lineCount;
    
    float alpha = step(uv.y, y);

    float scanline = step(sin((gl_FragCoord.y / u_scale.y) * 3.141), 0.5);


    float f = mix(mix(scanline, 1.0, alpha), mix(0.0, scanline, alpha), pass);


    FRAG_OUT = vec4(v_colour.rgb, f);
}
)";
