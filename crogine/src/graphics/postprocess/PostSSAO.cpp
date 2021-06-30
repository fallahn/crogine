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

#include <crogine/graphics/postprocess/PostSSAO.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>

#include "../../detail/GLCheck.hpp"
#include <crogine/detail/glm/gtx/compatibility.hpp>
#include <crogine/util/Random.hpp>

using namespace cro;

namespace
{
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

    //TODO add passes
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
void PostSSAO::apply(const RenderTexture& source)
{
#ifdef PLATFORM_DESKTOP
    //TODO activate ssao fbo
    //TODO render associated pass
    
    //TODO TODO blur pass
    
    
    //TODO activate main buffer
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
}