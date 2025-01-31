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

#include "../detail/stb_image.h"
#include "../detail/SDLImageRead.hpp"
#include "../detail/GLCheck.hpp"

#include <crogine/core/FileSystem.hpp>
#include <crogine/core/Log.hpp>

#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/Shader.hpp>

#include <array>
#include <vector>
#include <filesystem>

using namespace cro;

namespace
{
#include "shaders/PBRCubemap.hpp"
#include "shaders/PBRBRDF.hpp"

    //raii wrapper to delete temp textures
    //should an error occur
    struct TempTexture final
    {
        std::uint32_t handle = 0;

        ~TempTexture()
        {
            if (handle)
            {
                glCheck(glDeleteTextures(1, &handle));
            }
        }
    };

    //raii wrappers for frame buffer and render buffers
    struct TempFrameBuffer final
    {
        std::uint32_t handle = 0;

        ~TempFrameBuffer()
        {
            if (handle)
            {
                glCheck(glDeleteFramebuffers(1, &handle));
            }
        }
    };

    struct TempRenderBuffer final
    {
        std::uint32_t handle = 0;

        ~TempRenderBuffer()
        {
            if (handle)
            {
                glCheck(glDeleteRenderbuffers(1, &handle));
            }
        }
    };

    const std::uint32_t CubemapSize = 512u;
    const std::uint32_t IrradianceMapSize = 32u;
    const std::uint32_t PrefilterMapSize = 512u;
    const std::int32_t CubeVertCount = 36;

    const auto projectionMatrix = glm::perspective(90.f * cro::Util::Const::degToRad, 1.f, 0.1f, 2.f);
    const std::array viewMatrices =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
}

EnvironmentMap::EnvironmentMap()
    : m_cubeVBO         (0),
    m_cubeVAO           (0)
{
    std::fill(m_textures.begin(), m_textures.end(), 0);

    glCheck(glGenTextures(4, m_textures.data()));
}

EnvironmentMap::~EnvironmentMap()
{
    if (m_textures[Skybox])
    {
        glCheck(glDeleteTextures(4, m_textures.data()));
    }
    deleteCube();
}

//public
bool EnvironmentMap::loadFromFile(const std::string& filePath)
{
#ifdef PLATFORM_MOBILE
    LogE << "Environment mapping is not available on mobile platforms. Use a cubemap instead." << std::endl;
    return false;
#else

    std::string path;
    std::filesystem::path p(filePath);
    if (p.is_absolute())
    {
        path = filePath;
    }
    else
    {
        path = FileSystem::getResourcePath() + filePath;
    }

    if (!cro::FileSystem::fileExists(path))
    {
        LogE << path << ": File doesn't exist" << std::endl;
        return false;
    }

    auto* file = SDL_RWFromFile(path.c_str(), "rb");
    if (!file)
    {
        LogE << "SDLRW_ops Failed opening " << filePath << std::endl;
        return false;
    }

    STBIMG_stbio_RWops io;
    stbi_callback_from_RW(file, &io);

    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t componentCount = 0;

    TempTexture tempTexture;

    stbi_set_flip_vertically_on_load(1);
    auto* data = stbi_loadf_from_callbacks(&io.stb_cbs, &io, &width, &height, &componentCount, 0);
    if (data)
    {
        //store the image in a temp texture - we're going to write this to a cube map
        glCheck(glGenTextures(1, &tempTexture.handle));
        glCheck(glBindTexture(GL_TEXTURE_2D, tempTexture.handle));
        glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data));

        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

        stbi_image_free(data);
    }
    else
    {
        LogE << "STBI Failed opening " << filePath << ": " << stbi_failure_reason() << std::endl;
        
        return false;
    }
    stbi_set_flip_vertically_on_load(0);
    SDL_RWclose(file);

    //create a temp render buffer/frame buffer to render the sides with
    TempFrameBuffer tempFBO;
    TempRenderBuffer tempRBO;

    glCheck(glGenFramebuffers(1, &tempFBO.handle));
    glCheck(glGenRenderbuffers(1, &tempRBO.handle));

    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, tempFBO.handle));
    glCheck(glBindRenderbuffer(GL_RENDERBUFFER, tempRBO.handle));
    glCheck(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CubemapSize, CubemapSize));

    glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, tempRBO.handle));

    //create the cubemap which will be the skybox
    if (m_textures[Skybox] == 0)
    {
        LogE << "No valid textures were created." << std::endl;
        return false;
    }

    for (auto t : m_textures)
    {
        if (t == 0)
        {
            LogE << "Failed creating one or more textures" << std::endl;
            return false;
        }
    }

    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textures[Skybox]));

    for (auto i = 0u; i < 6u; ++i)
    {
        glCheck(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, CubemapSize, CubemapSize, 0, GL_RGB, GL_FLOAT, nullptr));
    }

    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    //render each side of the cube map
    cro::Shader shader;
    if (!shader.loadFromString(PBRCubeVertex, HDRToCubeFrag))
    {
        LogE << "Failed creating HDR to Cubemap shader" << std::endl;
        return false;
    }


    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glUniform1i(shader.getUniformID("u_hdrMap"), 0));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, tempTexture.handle));

    createCube();

    glCheck(glViewport(0, 0, CubemapSize, CubemapSize));
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, tempFBO.handle));
    for (auto i = 0u; i < 6u; ++i)
    {
        auto viewProj = projectionMatrix * viewMatrices[i];
        glCheck(glUniformMatrix4fv(shader.getUniformID("u_viewProjectionMatrix"), 1, GL_FALSE, &viewProj[0][0]));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_textures[Skybox], 0));

        glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        CRO_ASSERT(m_cubeVAO&& m_cubeVBO, "cube not correctly built!");
        //can't use VAOs on mobile - but this should be marked mobile incompatible anyway.
        glCheck(glBindVertexArray(m_cubeVAO));
        glCheck(glDrawArrays(GL_TRIANGLES, 0, CubeVertCount));
    }

    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textures[Skybox]));
    glCheck(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));

    if (!shader.loadFromString(PBRCubeVertex, IrradianceFrag))
    {
        LogE << "Failed creating irradiance convolution shader" << std::endl;
        return false;
    }

    renderIrradianceMap(tempFBO.handle, tempRBO.handle, shader);


    if (!shader.loadFromString(PBRCubeVertex, PrefilterFrag))
    {
        LogE << "Failed creating prefilter convolution shader" << std::endl;
        return false;
    }
    renderPrefilterMap(tempFBO.handle, tempRBO.handle, shader);


    if (!shader.loadFromString(BRDFVert, BRDFFrag))
    {
        LogE << "Failed creating BRDF shader" << std::endl;
        return false;
    }
    renderBRDFMap(tempFBO.handle, tempRBO.handle, shader);

    //make sure everything is put back neat :)
    glCheck(glBindVertexArray(0));
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    glCheck(glUseProgram(0));

    deleteCube();
    return true;

#endif //PLATFORM_MOBILE
}

void EnvironmentMap::renderIrradianceMap(std::uint32_t fbo, std::uint32_t rbo, Shader& shader)
{
    //create a smaller cube map to hold the irradiance map
    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textures[Irradiance]));

    for (auto i = 0u; i < 6u; ++i)
    {
        glCheck(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, IrradianceMapSize, IrradianceMapSize, 0, GL_RGB, GL_FLOAT, nullptr));
    }
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    //and rescale the FBO accordingly
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    glCheck(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
    glCheck(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IrradianceMapSize, IrradianceMapSize));

    //then render each face
    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glUniform1i(shader.getUniformID("u_environmentMap"), 0));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textures[Skybox]));
    //glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));

    glCheck(glViewport(0, 0, IrradianceMapSize, IrradianceMapSize));
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    for (auto i = 0u; i < 6u; ++i)
    {
        auto viewProj = projectionMatrix * viewMatrices[i];
        glCheck(glUniformMatrix4fv(shader.getUniformID("u_viewProjectionMatrix"), 1, GL_FALSE, &viewProj[0][0]));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_textures[Irradiance], 0));

        glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        CRO_ASSERT(m_cubeVAO && m_cubeVBO, "cube not correctly built!");
        glCheck(glBindVertexArray(m_cubeVAO));
        glCheck(glDrawArrays(GL_TRIANGLES, 0, CubeVertCount));
    }
}

void EnvironmentMap::renderPrefilterMap(std::uint32_t fbo, std::uint32_t rbo, Shader& shader)
{
    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textures[Prefilter]));

    for(auto i = 0u; i < 6u; ++i)
    {
        glCheck(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, PrefilterMapSize, PrefilterMapSize, 0, GL_RGB, GL_FLOAT, nullptr));
    }
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR)); // be sure to set minification filter to mip_linear 
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    glCheck(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));    

    //read from the skybox
    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glUniform1i(shader.getUniformID("u_environmentMap"), 0));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textures[Skybox]));


    //render multiple mip levels to vary the roughness
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    const std::uint32_t MaxLevels = 5;
    for (auto i = 0u; i < MaxLevels; ++i)
    {
        //resize the RBO for each level...
        auto mipSize = static_cast<std::uint32_t>(PrefilterMapSize * std::pow(0.5f, i));

        glCheck(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
        glCheck(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize));

        glViewport(0, 0, mipSize, mipSize);

        float roughness = static_cast<float>(i) / static_cast<float>(MaxLevels - 1);
        glCheck(glUniform1f(shader.getUniformID("u_roughness"), roughness));

        //render each side of this level
        for (auto j = 0; j < 6; ++j)
        {
            auto viewProj = projectionMatrix * viewMatrices[j];
            glCheck(glUniformMatrix4fv(shader.getUniformID("u_viewProjectionMatrix"), 1, GL_FALSE, &viewProj[0][0]));
            glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, m_textures[Prefilter], i));

            glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

            CRO_ASSERT(m_cubeVAO && m_cubeVBO, "cube not correctly built!");
            glCheck(glBindVertexArray(m_cubeVAO));
            glCheck(glDrawArrays(GL_TRIANGLES, 0, CubeVertCount));
        }
    }
}

void EnvironmentMap::renderBRDFMap(std::uint32_t fbo, std::uint32_t rbo, Shader& shader)
{
    glCheck(glBindTexture(GL_TEXTURE_2D, m_textures[BRDF]));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, CubemapSize, CubemapSize, 0, GL_RG, GL_FLOAT, 0));

    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    //update the fbo
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    glCheck(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
    glCheck(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CubemapSize, CubemapSize));
    glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textures[BRDF], 0));


    //create a quad
    std::uint32_t quadVBO = 0;
    std::uint32_t quadVAO = 0;

    std::vector<float> verts =
    {
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,
        1.0f,  1.0f, 0.0f,    1.0f, 1.0f,
        1.0f, -1.0f, 0.0f,    1.0f, 0.0f,
    };
    
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));


    //and render it...
    glCheck(glViewport(0, 0, CubemapSize, CubemapSize));
    
    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    glCheck(glBindVertexArray(quadVAO));
    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    glCheck(glDeleteBuffers(1, &quadVBO));
    glCheck(glDeleteVertexArrays(1, &quadVAO));
}

void EnvironmentMap::createCube()
{
    std::vector<float> verts =
    {
        //back
        -1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        //front
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        //left
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        //right
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        //bottom
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        //top
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f , 1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f
    };

    glGenBuffers(1, &m_cubeVBO);
    glGenVertexArrays(1, &m_cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindVertexArray(m_cubeVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void EnvironmentMap::deleteCube()
{
    if (m_cubeVBO)
    {
        glCheck(glDeleteBuffers(1, &m_cubeVBO));
        m_cubeVBO = 0;
    }

    if (m_cubeVAO)
    {
        glCheck(glDeleteVertexArrays(1, &m_cubeVAO));
        m_cubeVAO = 0;
    }
}