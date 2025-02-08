/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "MaskEditor.hpp"
#include "ErrorCheck.hpp"
#include "UIConsts.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/detail/glm/gtc/matrix_access.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    const std::string vertex = 
        R"(
            ATTRIBUTE vec2 a_position;
            ATTRIBUTE vec2 a_texCoord0;

            uniform mat4 u_projectionMatrix;

            VARYING_OUT vec2 v_texCoord0;

            void main()
            {
                v_texCoord0 = a_texCoord0;

                gl_Position = u_projectionMatrix * vec4(a_position, 0.0, 1.0);
            })";

    const std::string fragment =
        R"(
            uniform sampler2D u_metalMap;
            uniform sampler2D u_roughnessMap;
            uniform sampler2D u_aoMap;

            uniform float u_metalValue;
            uniform float u_roughnessValue;

            VARYING_IN vec2 v_texCoord0;
            OUTPUT

            void main()
            {
                FRAG_OUT.r = TEXTURE(u_metalMap, v_texCoord0).r * u_metalValue;
                FRAG_OUT.g = TEXTURE(u_roughnessMap, v_texCoord0).r * u_roughnessValue;
                FRAG_OUT.b = TEXTURE(u_aoMap, v_texCoord0).r;
                FRAG_OUT.a = 1.0;
            })";
}

MaskEditor::MaskEditor()
    : m_metalValue      (1.f),
    m_roughnessValue    (1.f)
{
    //create the textures
    cro::Image img;
    img.create(2, 2, cro::Colour::White);
    m_whiteTexture.loadFromImage(img);
    std::fill(m_quad.activeTextures.begin(), m_quad.activeTextures.end(), m_whiteTexture.getGLHandle());

    //create the shader
    std::fill(m_quad.uniforms.begin(), m_quad.uniforms.end(), -1);
    if (m_shader.loadFromString(vertex, fragment))
    {
        m_quad.uniforms[Quad::MetalTex] = m_shader.getUniformID("u_metalMap");
        m_quad.uniforms[Quad::RoughTex] = m_shader.getUniformID("u_roughnessMap");
        m_quad.uniforms[Quad::AOTex] = m_shader.getUniformID("u_aoMap");

        m_quad.uniforms[Quad::MetalVal] = m_shader.getUniformID("u_metalValue");
        m_quad.uniforms[Quad::RoughVal] = m_shader.getUniformID("u_roughnessValue");

        m_quad.uniforms[Quad::Projection] = m_shader.getUniformID("u_projectionMatrix");

        //TODO other uniforms
    }

    //create the quad
    static constexpr std::uint32_t VertexSize = 4 * sizeof(float);

    //create the vao/vbo
    glCheck(glGenBuffers(1, &m_quad.vbo));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_quad.vbo));

    std::vector<float> verts =
    {
        0.f, 1024.f,    0.f, 1.f,
        0.f, 0.f,       0.f, 0.f,
        1024.f, 1024.f, 1.f, 1.f,
        1024.f, 0.f,    1.f, 0.f
    };
    glCheck(glBufferData(GL_ARRAY_BUFFER, VertexSize * verts.size(), verts.data(), GL_STATIC_DRAW));

    //bind the material properties
    glCheck(glGenVertexArrays(1, &m_quad.vao));
    glCheck(glBindVertexArray(m_quad.vao));

    const auto& attribs = m_shader.getAttribMap();
    glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Attribute::Position]));
    glCheck(glVertexAttribPointer(attribs[cro::Mesh::Attribute::Position], 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(VertexSize), reinterpret_cast<void*>(static_cast<intptr_t>(0))));

    glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Attribute::UV0]));
    glCheck(glVertexAttribPointer(attribs[cro::Mesh::Attribute::UV0], 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(VertexSize), reinterpret_cast<void*>(static_cast<intptr_t>(2 * sizeof(float)))));

    glCheck(glBindVertexArray(0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    m_quad.projectionMatrix = glm::ortho(0.f, 1024.f, 0.f, 1024.f, -1.f, 2.f);

    //create the output
    m_outputTexture.create(1024, 1024, false);
    m_outputTexture.clear(cro::Colour::White);
    m_outputTexture.display();
}

MaskEditor::~MaskEditor()
{
    //tidy up quad
    if (m_quad.vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_quad.vao));
    }

    if (m_quad.vbo)
    {
        glCheck(glDeleteBuffers(1, &m_quad.vbo));
    }
}

//public
void MaskEditor::doImGui(bool* open)
{
    if (!*open)
    {
        return;
    }

    ImGui::SetNextWindowSize({ 550.f, 550.f });
    if (ImGui::Begin("Mask Editor", open))
    {
        if (ImGui::SliderFloat("Metalness", &m_metalValue, 0.f, 1.f))
        {
            updateOutput();
        }
        if (ImGui::SliderFloat("Roughness", &m_roughnessValue, 0.f, 1.f))
        {
            updateOutput();
        }

        //image buttons want to use the image ID handle as UID so we need to push our
        //own values because we want to share image IDs over multiple buttons
        ImGui::PushID(100);
        if (ImGui::ImageButton(m_quad.activeTextures[ChannelID::Metal], { 64.f, 64.f }, { 0.f, 1.f }, { 1.f, 0.f }))
        {
            auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
            if (!path.empty()
                && m_channelTextures[ChannelID::Metal].loadFromFile(path))
            {
                m_quad.activeTextures[ChannelID::Metal] = m_channelTextures[ChannelID::Metal].getGLHandle();
                updateOutput();
            }
        }
        ImGui::PopID();
        uiConst::showTipMessage("Click to select the metalness map");
        ImGui::SameLine();

        ImGui::PushID(101);
        if (ImGui::ImageButton(m_quad.activeTextures[ChannelID::Roughness], { 64.f, 64.f }, { 0.f, 1.f }, { 1.f, 0.f }))
        {
            auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
            if (!path.empty()
                && m_channelTextures[ChannelID::Roughness].loadFromFile(path))
            {
                m_quad.activeTextures[ChannelID::Roughness] = m_channelTextures[ChannelID::Roughness].getGLHandle();
                updateOutput();
            }
        }
        ImGui::PopID();
        uiConst::showTipMessage("Click to select the roughness map");
        ImGui::SameLine();

        ImGui::PushID(102);
        if (ImGui::ImageButton(m_quad.activeTextures[ChannelID::AO], { 64.f, 64.f }, { 0.f, 1.f }, { 1.f, 0.f }))
        {
            auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
            if (!path.empty()
                && m_channelTextures[ChannelID::AO].loadFromFile(path))
            {
                m_quad.activeTextures[ChannelID::AO] = m_channelTextures[ChannelID::AO].getGLHandle();
                updateOutput();
            }
        }
        ImGui::PopID();
        uiConst::showTipMessage("Click to select the ambient occlusion map");

        if (ImGui::Button("Clear", { 72.f, 20.f }))
        {
            m_quad.activeTextures[ChannelID::Metal] = m_whiteTexture.getGLHandle();
            updateOutput();
        }
        uiConst::showTipMessage("Clears the metal map");
        ImGui::SameLine();
        if (ImGui::Button("Clear##Roughness", { 72.f, 20.f }))
        {
            m_quad.activeTextures[ChannelID::Roughness] = m_whiteTexture.getGLHandle();
            updateOutput();
        }
        uiConst::showTipMessage("Clears the roughness map");
        ImGui::SameLine();
        if (ImGui::Button("Clear##Ambient", { 72.f, 20.f }))
        {
            m_quad.activeTextures[ChannelID::AO] = m_whiteTexture.getGLHandle();
            updateOutput();
        }
        uiConst::showTipMessage("Clears the ambient occlusion map");
        ImGui::SameLine();
        if (ImGui::Button("Clear All"))
        {
            for (auto& t : m_quad.activeTextures)
            {
                t = m_whiteTexture.getGLHandle();
            }
            updateOutput();
        }

        ImGui::NewLine();

        ImGui::Text("TODO: select the resolution here. (currently 1024x1024)");
        /*if (ImGui::BeginCombo("##Shader", ShaderStrings[matDef.type]))
        {
            for (auto i = 0u; i < ShaderStrings.size(); ++i)
            {
                bool selected = matDef.type == i;
                if (ImGui::Selectable(ShaderStrings[i], selected))
                {
                    matDef.type = static_cast<MaterialDefinition::Type>(i);
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }*/

        ImGui::SameLine();
        if (ImGui::Button("Save File"))
        {
            auto path = cro::FileSystem::saveFileDialogue("", "png");
            if (!path.empty())
            {
                m_outputTexture.saveToFile(path);
            }
        }

        ImGui::Image(m_outputTexture.getTexture(), { 512.f, 512.f }, { 0.f, 1.f }, { 1.f, 0.f });
    }
    ImGui::End();
}

//private
void MaskEditor::updateOutput()
{
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_quad.activeTextures[ChannelID::Metal]));

    glCheck(glActiveTexture(GL_TEXTURE1));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_quad.activeTextures[ChannelID::Roughness]));

    glCheck(glActiveTexture(GL_TEXTURE2));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_quad.activeTextures[ChannelID::AO]));

    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniform1i(m_quad.uniforms[Quad::MetalTex], 0));
    glCheck(glUniform1i(m_quad.uniforms[Quad::RoughTex], 1));
    glCheck(glUniform1i(m_quad.uniforms[Quad::AOTex], 2));
    glCheck(glUniform1f(m_quad.uniforms[Quad::MetalVal], m_metalValue));
    glCheck(glUniform1f(m_quad.uniforms[Quad::RoughVal], m_roughnessValue));
    glCheck(glUniformMatrix4fv(m_quad.uniforms[Quad::Projection], 1, GL_FALSE, &m_quad.projectionMatrix[0][0]));

    glCheck(glBindVertexArray(m_quad.vao));

    m_outputTexture.clear();
    glCheck(glDepthMask(GL_FALSE));

    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    glCheck(glDepthMask(GL_TRUE));
    m_outputTexture.display();

    glCheck(glBindVertexArray(0));
    glCheck(glUseProgram(0));
}