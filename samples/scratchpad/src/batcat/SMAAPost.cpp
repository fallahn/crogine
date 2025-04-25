/*-----------------------------------------------------------------------

Matt Marchant 2025
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "SMAAPost.hpp"
#include "AreaTex.h"
#include "SearchTex.h"

#include <crogine/detail/OpenGL.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/Gui.hpp>
#endif

namespace
{
#include "smaa_shaders/edges.inl"
#include "smaa_shaders/weights.inl"
#include "smaa_shaders/blend.inl"

    void setResolution(cro::Shader& shader, glm::vec2 resolution)
    {
        glUseProgram(shader.getGLHandle());
        glUniform2f(shader.getUniformID("u_resolution"), resolution.x, resolution.y);
    }

    constexpr std::int32_t TextureSlotOffset = 5;
}

SMAAPost::SMAAPost()
    : m_inputTexture    (0),
    m_output            (nullptr)
{
    std::fill(m_supportTextures.begin(), m_supportTextures.end(), 0);

    static constexpr auto RowSize = AREATEX_PITCH;

    std::vector<unsigned char> flipBuffer;
    for (auto y = AREATEX_HEIGHT - 1; y >= 0; y--)
    {
        for (auto x = 0; x < RowSize; x += 2)
        //for (auto x = RowSize - 2; x >= 0; x -= 2)
        {
            const auto idx = (RowSize * y) + x;
            flipBuffer.push_back(areaTexBytes[idx]);
            flipBuffer.push_back(areaTexBytes[idx+1]);
        }
    }

    glGenTextures(2, m_supportTextures.data());
    glBindTexture(GL_TEXTURE_2D, m_supportTextures[TextureID::Area]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, AREATEX_WIDTH, AREATEX_HEIGHT, 0, GL_RG, GL_UNSIGNED_BYTE, flipBuffer.data());
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, AREATEX_WIDTH, AREATEX_HEIGHT, 0, GL_RG, GL_UNSIGNED_BYTE, areaTexBytes);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    flipBuffer.clear();
    for (auto y = SEARCHTEX_HEIGHT - 1; y >= 0; y--)
    {
        for (auto x = 0; x < SEARCHTEX_WIDTH; ++x)
        //for (auto x = SEARCHTEX_WIDTH - 1; x >= 0; x--)
        {
            const auto idx = (SEARCHTEX_WIDTH * y) + x;
            flipBuffer.push_back(searchTexBytes[idx]);
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_supportTextures[TextureID::Search]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, flipBuffer.data());
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, searchTexBytes);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);


    //define SMAA_PRESET_LOW
    //define SMAA_PRESET_MEDIUM
    //define SMAA_PRESET_HIGH
    //define SMAA_PRESET_ULTRA
    const std::string quality = "#define SMAA_PRESET_ULTRA\n";
    m_edgeShader.loadFromString(SMAA::Edge::Vert, SMAA::Edge::LumaFragment, quality);
    m_weightShader.loadFromString(SMAA::Weights::Vert, SMAA::Weights::Frag, quality);
    m_blendShader.loadFromString(SMAA::Blend::Vert, SMAA::Blend::Frag, quality);

#ifdef CRO_DEBUG_
    registerWindow([&]()
        {
            if (ImGui::Begin("SMAA Textures"))
            {
                ImGui::Image(cro::TextureID(m_supportTextures[TextureID::Area]), { AREATEX_WIDTH, AREATEX_HEIGHT }, { 0.f, 1.f }, { 1.f, 0.f });
                ImGui::SameLine();
                ImGui::Image(cro::TextureID(m_supportTextures[TextureID::Search]), { SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT }, { 0.f, 1.f }, { 1.f, 0.f });

                ImGui::Image(m_blendTexture.getTexture(), { 200.f, 150.f }, { 0.f, 1.f }, { 1.f, 0.f });
            }
            ImGui::End();
        });  
#endif
}

SMAAPost::~SMAAPost()
{
    if (m_supportTextures[0])
    {
        glDeleteTextures(2, m_supportTextures.data());
    }
}

//public
void SMAAPost::create(const cro::Texture& input, cro::RenderTexture& output)
{
    CRO_ASSERT(input.getSize() == output.getSize(), "");

    m_inputTexture = input.getGLHandle();
    m_output = &output;

    const auto size = output.getSize();
    m_edgeTexture.create(size.x, size.y, false);
    m_edgeTexture.setSmooth(true); //this MUST be true else weight calc fails
    m_edgeTexture.setRepeated(false);

    m_blendTexture.create(size.x, size.y, false);
    m_blendTexture.setSmooth(true);
    m_blendTexture.setRepeated(false);


    m_edgeDetection.setTexture(input);
    m_edgeDetection.setBlendMode(cro::Material::BlendMode::None);
    m_edgeDetection.setShader(m_edgeShader);
    setResolution(m_edgeShader, glm::vec2(size));


    m_blendCalc.setTexture(m_edgeTexture.getTexture());
    m_blendCalc.setBlendMode(cro::Material::BlendMode::None);
    m_blendCalc.setShader(m_weightShader);
    setResolution(m_weightShader, glm::vec2(size));
    glUseProgram(m_weightShader.getGLHandle());
    glUniform1i(m_weightShader.getUniformID("u_areaTexture"), TextureID::Area + TextureSlotOffset);
    glUniform1i(m_weightShader.getUniformID("u_searchTexture"), TextureID::Search + TextureSlotOffset);


    m_blendOutput.setTexture(m_blendTexture.getTexture());
    m_blendOutput.setBlendMode(cro::Material::BlendMode::None);
    m_blendOutput.setShader(m_blendShader);
    setResolution(m_blendShader, size);
    glUseProgram(m_blendShader.getGLHandle());
    glUniform1i(m_blendShader.getUniformID("u_colourTexture"), TextureSlotOffset);
    glUseProgram(0);
}

void SMAAPost::render()
{
    m_edgeTexture.clear(cro::Colour::Transparent);
    m_edgeDetection.draw();
    m_edgeTexture.display();

    //don't set these as zero cos it clashes with default texture...
    glActiveTexture(GL_TEXTURE0 + TextureSlotOffset);
    glBindTexture(GL_TEXTURE_2D, m_supportTextures[0]);
    glActiveTexture(GL_TEXTURE1 + TextureSlotOffset);
    glBindTexture(GL_TEXTURE_2D, m_supportTextures[1]);

    m_blendTexture.clear(cro::Colour::Transparent);
    m_blendCalc.draw();
    m_blendTexture.display();


    glActiveTexture(GL_TEXTURE0 + TextureSlotOffset);
    glBindTexture(GL_TEXTURE_2D, m_inputTexture);

    m_output->clear(cro::Colour::Transparent);
    m_blendOutput.draw();
    m_output->display();
}