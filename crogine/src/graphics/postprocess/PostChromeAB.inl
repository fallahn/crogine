/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

static const std::string FragChromeAB = R"(
    uniform sampler2D u_input;

    varying MED vec2 v_texCoord;

    const float maxOffset = 1.0 / 300.0; //decrease the ratio to create a bigger effect

    void main()
    {
        MED vec2 texCoord = v_texCoord;
        MED vec2 offset = vec2((maxOffset / 2.0) - (texCoord.x * maxOffset), (maxOffset / 2.0) - (texCoord.y * maxOffset));

        MED vec3 colour = vec3(0.0);
        colour.r = texture2D(u_input, texCoord + offset).r;
        colour.g = texture2D(u_input, texCoord).g;
        colour.b = texture2D(u_input, texCoord - offset).b;

        gl_FragColor.rgb = colour;
    }
)";