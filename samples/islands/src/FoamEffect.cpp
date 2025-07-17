/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "FoamEffect.hpp"

#include <crogine/detail/OpenGL.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/QuadBuilder.hpp>

#include <string>

namespace
{
    const std::uint32_t BufferSize = 256;
    const float QuadSize = 2.f;

    const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE vec2 a_texCoord0;

        VARYING_OUT vec2 v_texCoord;

        void main()
        {
            gl_Position = a_position;

            v_texCoord = a_texCoord0;

        })";

    //https://www.shadertoy.com/view/4djGRh

    const std::string Fragment = R"(
        OUTPUT

        uniform float u_time;
        uniform sampler2D u_texture;

        VARYING_IN vec2 v_texCoord;

        const float CellCount = 8.0;

        vec2 noise(vec2 point)
        {
            point -= floor(point / 289.0) * 289.0;
            point += vec2(223.35734, 550.56781);
            point *= point;
    
            float xy = point.x * point.y;
    
            return vec2(fract(xy * 0.00000012), fract(xy * 0.00000543));
        }

        vec2 Hash2(vec2 point)
        {
            //float r = 523.0 * sin(dot(point, vec2(53.3158, 43.6143)));
            //return vec2(fract(15.32354 * r), fract(17.25865 * r));

            float time = u_time * 0.00003;
            //return TEXTURE(u_texture, point * vec2(0.135 + time, 0.2325 - time), -100.0).rg;
            return noise(point * vec2(0.135 + time, 0.2325 - time));
        }

        float cells(vec2 point)
        {
            point *= CellCount;
            float d = 1.0e10;

            for(int x = -1; x <= 1; ++x)
            {
                for(int y = -1; y <= 1; ++y)
                {
                    vec2 tp = floor(point) + vec2(x, y);
                    tp = point - tp - Hash2(mod(tp, CellCount));
                    d = min(d, dot(tp, tp));
                }
            }
            return sqrt(d);
        }

        void main()
        {        
            //FRAG_OUT = vec4(v_texCoord.x, v_texCoord.y, 0.0, 1.0);

            //float c = cells(v_texCoord);
            //FRAG_OUT = vec4(c * 0.83, c, min(1.0, c * 1.3), 1.0);

            vec2 q = v_texCoord;;
            q.x += 2.0;
            vec2 p = 10.0 * q * mat2(0.7071, -0.7071, 0.7071, 0.7071);
    
            vec2 pi = floor(p);    
            vec4 v = vec4( pi.xy, pi.xy + 1.0);
            v -= 64.0 * floor(v * 0.015);
            v.xz = v.xz * 1.435 + 34.423;
            v.yw = v.yw * 2.349 + 183.37;
            v = v.xzxz * v.yyww;
            v *= v;
    
            v *= u_time * 0.000004 + 0.5;
            vec4 vx = 0.25 * sin(fract(v * 0.00047) * 6.2831853);
            vec4 vy = 0.25 * sin(fract(v * 0.00074) * 6.2831853);
    
            vec2 pf = p - pi;
            vx += vec4(0.0, 1.0, 0.0, 1.0) - pf.xxxx;
            vy += vec4(0.0, 0.0, 1.0, 1.0) - pf.yyyy;
            v = vx * vx + vy * vy;
    
            v.xy = min(v.xy, v.zw);
            vec3 col = mix(vec3(0.0, 0.0, 0.0), vec3(0.89, 0.895, 0.895), min(v.x, v.y) );     
            FRAG_OUT = vec4(col, 1.0);
        })";
}

FoamEffect::FoamEffect(cro::ResourceCollection& rc)
    : m_textureUniform   (0)
{
    if (m_shader.loadFromString(Vertex, Fragment))
    {
        const auto& uniforms = m_shader.getUniformMap();

        if (uniforms.count("u_texture") != 0)
        {
            m_textureUniform = m_shader.getUniformID("u_texture");
        }

        if(uniforms.count("u_time") != 0)
        {
            m_timeUniform = m_shader.getUniformID("u_time");
        }
    }

    auto meshID = rc.meshes.loadMesh(cro::QuadBuilder(glm::vec2(QuadSize)));
    auto matID = rc.materials.add(m_shader);

    m_model = { rc.meshes.getMesh(meshID), rc.materials.get(matID) };

    m_texture.create(BufferSize, BufferSize, false);
    m_texture.setViewport({ 0,0,BufferSize,BufferSize });
    m_texture.setRepeated(true);

    //m_noiseTexture.loadFromFile("assets/images/noise.png");
}

void FoamEffect::update()
{
    //let's just assume we're not rendering on mobile in this case
    m_texture.clear(cro::Colour::Green);

    glEnable(GL_CULL_FACE);
    glUseProgram(m_shader.getGLHandle());
    glUniform1f(m_timeUniform, m_timer.elapsed().asSeconds());

    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, m_noiseTexture.getGLHandle());
    //glUniform1i(m_textureUniform, 0);
    

    m_model.draw(0, cro::Mesh::IndexData::Final);

    glBindVertexArray(0);

    glUseProgram(0);
    glDisable(GL_CULL_FACE);
    m_texture.display();
}