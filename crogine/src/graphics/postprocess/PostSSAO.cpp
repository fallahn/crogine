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
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/util/Random.hpp>

using namespace cro;

namespace
{
#include "PostSSAO.inl"

    constexpr std::int32_t KernelSize = 32;
    constexpr std::size_t NoiseSize = 16;
}

PostSSAO::PostSSAO(const MultiRenderTexture& mrt)
    : m_mrt         (mrt),
    m_noiseTexture  (0),
    m_ssaoTexture   (0),
    m_blurTexture   (0),
    m_ssaoFBO       (0),
    m_blurFBO       (0),
    m_intensity     (0.5f),
    m_bias          (0.001f)
{
#ifdef PLATFORM_DESKTOP
    //create noise texture/samples
    createNoiseSampler();

    //load shaders
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

        if (uniforms.count("u_bufferViewport"))
        {
            m_ssaoUniforms[SSAOUniformID::BufferViewport] = uniforms.at("u_bufferViewport");
        }

        if (uniforms.count("u_bias"))
        {
            m_ssaoUniforms[SSAOUniformID::Bias] = uniforms.at("u_bias");
        }

        if (uniforms.count("u_intensity"))
        {
            m_ssaoUniforms[SSAOUniformID::Intensity] = uniforms.at("u_intensity");
        }
    }

    std::fill(m_blurUniforms.begin(), m_blurUniforms.end(), -1);
    if (m_blurShader.loadFromString(PostVertex, BlurFrag2))
    {
        const auto& uniforms = m_blurShader.getUniformMap();
        if (uniforms.count("u_texture"))
        {
            m_blurUniforms[BlurUniformID::Texture] = uniforms.at("u_texture");
        }

        if (uniforms.count("u_bufferSize"))
        {
            m_blurUniforms[BlurUniformID::BufferSize] = uniforms.at("u_bufferSize");
        }
    }

    std::fill(m_blendUniforms.begin(), m_blendUniforms.end(), -1);
    if (m_blendShader.loadFromString(PostVertex, BlendFrag))
    {
        const auto& uniforms = m_blendShader.getUniformMap();
        if (uniforms.count("u_baseTexture"))
        {
            m_blendUniforms[BlendUniformID::Base] = uniforms.at("u_baseTexture");
        }

        if (uniforms.count("u_ssaoTexture"))
        {
            m_blendUniforms[BlendUniformID::SSAO] = uniforms.at("u_ssaoTexture");
        }
    }

    //add passes
    m_passIDs[PassID::SSAO] = addPass(m_ssaoShader);
    m_passIDs[PassID::Blur] = addPass(m_blurShader);
    m_passIDs[PassID::Final] = addPass(m_blendShader);
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

    if (m_blurTexture)
    {
        glCheck(glDeleteFramebuffers(1, &m_blurFBO));
        glCheck(glDeleteTextures(1, &m_blurTexture));
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

    //TODO we could optomise this a bit by only setting the
    //texture uniforms once and the bias/intensity only when they're changed
    glCheck(glUseProgram(m_ssaoShader.getGLHandle()));
    glCheck(glUniform1i(m_ssaoUniforms[SSAOUniformID::Normal], 0));
    glCheck(glUniform1i(m_ssaoUniforms[SSAOUniformID::Position], 1));
    glCheck(glUniform1i(m_ssaoUniforms[SSAOUniformID::Noise], 2));
    glCheck(glUniform3fv(m_ssaoUniforms[SSAOUniformID::Samples], KernelSize, glm::value_ptr(m_kernel[0])));
    glCheck(glUniform2f(m_ssaoUniforms[SSAOUniformID::BufferSize], m_bufferSize.x, m_bufferSize.y));
    glCheck(glUniform4f(m_ssaoUniforms[SSAOUniformID::BufferViewport], camera.viewport.left, camera.viewport.bottom, camera.viewport.width,camera.viewport.height));
    glCheck(glUniformMatrix4fv(m_ssaoUniforms[SSAOUniformID::ProjectionMatrix], 1, GL_FALSE, &camera.getProjectionMatrix()[0][0]));
    glCheck(glUniform1f(m_ssaoUniforms[SSAOUniformID::Bias], m_bias));
    glCheck(glUniform1f(m_ssaoUniforms[SSAOUniformID::Intensity], m_intensity));

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


    //-----blur pass-----//
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_ssaoTexture));

    glCheck(glUseProgram(m_blurShader.getGLHandle()));
    glCheck(glUniform1i(m_blurUniforms[BlurUniformID::Texture], 0));
    glCheck(glUniform2f(m_blurUniforms[BlurUniformID::BufferSize], m_bufferSize.x, m_bufferSize.y));
    
    //TODO if we resize this make sure to update the viewport
    glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_blurFBO));
    glClear(GL_COLOR_BUFFER_BIT);

    drawQuad(m_passIDs[PassID::Blur], { glm::vec2(0.f), m_bufferSize });


    //-----final pass------//
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, source.getTexture().getGLHandle()));

    glCheck(glActiveTexture(GL_TEXTURE1));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_blurTexture));

    glCheck(glUseProgram(m_blendShader.getGLHandle()));
    glCheck(glUniform1i(m_blendUniforms[BlendUniformID::Base], 0));
    glCheck(glUniform1i(m_blendUniforms[BlendUniformID::SSAO], 1));


    //activate original buffer
    glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, currentBinding));
    glCheck(glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]));

    //render final pass
    drawQuad(m_passIDs[PassID::Final], { glm::vec2(0.f), glm::vec2(App::getWindow().getSize()) });

#endif
}

void PostSSAO::setIntensity(float amount)
{
    m_intensity = std::min(5.f, std::max(0.1f, amount));
}

void PostSSAO::setBias(float amount)
{
    m_bias = std::min(0.1f, std::max(0.001f, amount));
}

//private
void PostSSAO::createNoiseSampler()
{
    constexpr glm::vec3 normal(0.f, 0.f, 1.f);
    constexpr std::int32_t MaxTries = KernelSize * 4;
    for (auto i = 0, j = 0; i < KernelSize && j < MaxTries; ++j)
    {
        auto sample = glm::vec3(
            cro::Util::Random::value(-1.f, 1.f),
            cro::Util::Random::value(-1.f, 1.f),
            cro::Util::Random::value(0.f, 1.f) );

        sample = glm::normalize(sample);

        //don't inclue low-angle normals
        if (glm::dot(sample, normal) > 0.15f)
        {
            sample *= cro::Util::Random::value(0.f, 1.f);

            float scale = static_cast<float>(i) / KernelSize;
            scale = glm::lerp(0.1f, 1.f, scale * scale);
            sample *= scale;

            m_kernel.push_back(sample);
            i++;
        }
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

    //TODO reduce size? Looks pretty awful, could make this a
    //performance preference

    //create SSAO buffer
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

    glCheck(glBindTexture(GL_TEXTURE_2D, m_ssaoTexture));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, size.x, size.y, 0, GL_RED, GL_FLOAT, nullptr));


    //create blur buffer
    if (m_blurFBO == 0)
    {
        glCheck(glGenFramebuffers(1, &m_blurFBO));
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_blurFBO));

        glCheck(glGenTextures(1, &m_blurTexture));
        glCheck(glBindTexture(GL_TEXTURE_2D, m_blurTexture));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_blurTexture, 0));
    }

    glCheck(glBindTexture(GL_TEXTURE_2D, m_blurTexture));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, size.x, size.y, 0, GL_RED, GL_FLOAT, nullptr));

    m_bufferSize = size;
}