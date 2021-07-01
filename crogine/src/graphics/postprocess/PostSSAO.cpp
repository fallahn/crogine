/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include <crogine/ecs/components/Camera.hpp>

#include <crogine/graphics/postprocess/PostSSAO.hpp>
#include <crogine/graphics/postprocess/PostVertex.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>

#include "../../detail/GLCheck.hpp"
#include <crogine/detail/glm/gtx/compatibility.hpp>
#include <crogine/util/Random.hpp>

using namespace cro;

namespace
{
#include "PostSSAO.inl"

    constexpr std::size_t KernelSize = 64;
    constexpr std::size_t NoiseSize = 16;
}

PostSSAO::PostSSAO(const MultiRenderTexture& mrt)
    : m_mrt         (mrt),
    m_noiseTexture  (0),
    m_ssaoTexture   (0),
    m_ssaoFBO       (0)
{
#ifdef PLATFORM_DESKTOP
    //create noise texture/samples
    createNoiseSampler();

    //TODO load shaders
    std::fill(m_ssaoUniforms.begin(), m_ssaoUniforms.end(), -1);
    if (m_ssaoShader.loadFromString(PostVertex, SSAOFrag))
    {
        //parse uniforms
        const auto& uniforms = m_ssaoShader.getUniformMap();
        if (uniforms.count("u_position"))
        {
            m_ssaoUniforms[SSAOUniformID::Position] = uniforms.at("u_position");
        }

        if (uniforms.count("u_normal"))
        {
            m_ssaoUniforms[SSAOUniformID::Normal] = uniforms.at("u_normal");
        }

        if (uniforms.count("u_noise"))
        {
            m_ssaoUniforms[SSAOUniformID::Noise] = uniforms.at("u_noise");
        }

        if (uniforms.count("u_samples[0]"))
        {
            m_ssaoUniforms[SSAOUniformID::Samples] = uniforms.at("u_samples[0]");
        }

        if (uniforms.count("u_camProjectionMatrix"))
        {
            m_ssaoUniforms[SSAOUniformID::ProjectionMatrix] = uniforms.at("u_camProjectionMatrix");
        }

        if (uniforms.count("u_bufferSize"))
        {
            m_ssaoUniforms[SSAOUniformID::BufferSize] = uniforms.at("u_bufferSize");
        }
    }

    //TODO add passes
    m_passIDs[PassID::SSAO] = addPass(m_ssaoShader);
#endif
}

PostSSAO::~PostSSAO()
{
#ifdef PLATFORM_DESKTOP

    if (m_noiseTexture)
    {
        glCheck(glDeleteTextures(1, &m_noiseTexture));
    }

    if (m_ssaoFBO)
    {
        glCheck(glDeleteFramebuffers(1, &m_ssaoFBO));
        glCheck(glDeleteTextures(1, &m_ssaoTexture));
    }

#endif
}

//public
void PostSSAO::apply(const RenderTexture& source, const Camera& camera)
{
#ifdef PLATFORM_DESKTOP
    //-----SSAO Pass-----//
    
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_mrt.getTexture(0).textureID));
    
    glCheck(glActiveTexture(GL_TEXTURE1));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_mrt.getTexture(1).textureID));

    glCheck(glActiveTexture(GL_TEXTURE2));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_noiseTexture));

    glCheck(glUseProgram(m_ssaoShader.getGLHandle()));
    glCheck(glUniform1i(m_ssaoUniforms[SSAOUniformID::Normal], 0));
    glCheck(glUniform1i(m_ssaoUniforms[SSAOUniformID::Position], 1));
    glCheck(glUniform1i(m_ssaoUniforms[SSAOUniformID::Noise], 2));
    glCheck(glUniform3fv(m_ssaoUniforms[SSAOUniformID::Samples], KernelSize, &m_kernel.data()[0][0]));
    glCheck(glUniform2f(m_ssaoUniforms[SSAOUniformID::BufferSize], m_bufferSize.x, m_bufferSize.y));
    glCheck(glUniformMatrix4fv(m_ssaoUniforms[SSAOUniformID::ProjectionMatrix], 1, GL_FALSE, &camera.getProjectionMatrix()[0][0]));

    //activate ssao fbo
    GLint currentBinding = 0;
    glCheck(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentBinding));
    glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_ssaoFBO));
    glClear(GL_COLOR_BUFFER_BIT);

    std::array<std::int32_t, 4u> lastViewport;
    glCheck(glGetIntegerv(GL_VIEWPORT, lastViewport.data()));
    glCheck(glViewport(0, 0, static_cast<std::int32_t>(m_bufferSize.x), static_cast<std::int32_t>(m_bufferSize.y)));

    //render associated pass
    drawQuad(m_passIDs[PassID::SSAO], { glm::vec2(0.f), m_bufferSize });


    //TODO TODO blur pass
    
    
    //activate main buffer
    glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, currentBinding));
    glCheck(glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]));

    //TODO render final pass

#endif
}

//private
void PostSSAO::createNoiseSampler()
{
    for (auto i = 0u; i < KernelSize; ++i)
    {
        auto sample = m_kernel.emplace_back(
            cro::Util::Random::value(-1.f, 1.f),
            cro::Util::Random::value(-1.f, 1.f),
            cro::Util::Random::value(0.f, 1.f) );

        sample = glm::normalize(sample);
        sample *= cro::Util::Random::value(0.f, 1.f);

        float scale = static_cast<float>(i) / KernelSize;
        scale = glm::lerp(0.1f, 1.f, scale * scale);
        sample *= scale;
    }

    std::vector<glm::vec3> noise;
    for (auto i = 0u; i < NoiseSize; ++i)
    {
        noise.emplace_back(
            cro::Util::Random::value(-1.f, 1.f),
            cro::Util::Random::value(-1.f, 1.f),
            0.f);
    }

    glCheck(glGenTextures(1, &m_noiseTexture));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_noiseTexture));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data()));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
}

void PostSSAO::bufferResized()
{
    auto size = getCurrentBufferSize();

    //TODO reduce size ?

    if (m_ssaoFBO == 0)
    {
        glCheck(glGenFramebuffers(1, &m_ssaoFBO));
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO));

        glCheck(glGenTextures(1, &m_ssaoTexture));
        glCheck(glBindTexture(GL_TEXTURE_2D, m_ssaoTexture));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoTexture, 0));
    }

    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, size.x, size.y, 0, GL_RED, GL_FLOAT, nullptr));


    //TODO create and update blur buffer


    m_bufferSize = size;
}