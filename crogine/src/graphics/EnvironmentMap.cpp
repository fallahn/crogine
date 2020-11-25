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

using namespace cro;

namespace
{
    const std::string HDRToCubeVertex = R"(
        attribute vec4 a_position;

        VARYING_OUT vec3 v_worldPosition;

        uniform mat4 u_viewProjectionMatrix;

        void main()
        {
            v_worldPosition = a_position.xyz;
            gl_Position =  u_viewProjectionMatrix * a_position;
        })";

    const std::string HDRToCubeFrag = R"(
        OUTPUT
        VARYING_IN vec3 v_worldPosition;

        uniform sampler2D u_hdrMap;

        const vec2 invAtan = vec2(0.1591, 0.3183);
        vec2 sampleSphericalMap(vec3 v)
        {
            vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
            uv *= invAtan;
            uv += 0.5;
            return uv;
        }

        void main()
        {		
            vec2 uv = sampleSphericalMap(normalize(v_worldPosition));
            vec3 colour = TEXTURE(u_hdrMap, uv).rgb;
    
            FRAG_OUT = vec4(colour, 1.0);
        })";

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

    const std::uint32_t CubemapSize = 1024u;
}

EnvironmentMap::EnvironmentMap()
    : m_skyboxTexture   (0),
    m_irradianceTexture (0),
    m_cubeVBO           (0),
    m_cubeVAO           (0)
{

}

EnvironmentMap::~EnvironmentMap()
{
    if (m_skyboxTexture)
    {
        glCheck(glDeleteTextures(1, &m_skyboxTexture));
    }

    if (m_irradianceTexture)
    {
        glCheck(glDeleteTextures(1, &m_irradianceTexture));
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

    auto path = FileSystem::getResourcePath() + filePath;

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
    //auto* data = stbi_loadf(path.c_str(), &width, &height, &componentCount, 0);
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
    if (m_skyboxTexture == 0)
    {
        glCheck(glGenTextures(1, &m_skyboxTexture));
    }
    glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyboxTexture));

    for (auto i = 0; i < 6u; ++i)
    {
        glCheck(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, CubemapSize, CubemapSize, 0, GL_RGB, GL_FLOAT, nullptr));
    }

    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    //render each side of the cube map
    auto projectionMatrix = glm::perspective(90.f * cro::Util::Const::degToRad, 1.f, 0.1f, 2.f);
    std::array viewMatrices =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    cro::Shader shader;
    if (!shader.loadFromString(HDRToCubeVertex, HDRToCubeFrag))
    {
        LogE << "Failed creating HDR to Cubemap shader" << std::endl;
        return false;
    }

    const auto& uniforms = shader.getUniformMap();
    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glUniform1i(uniforms.at("u_hdrMap"), 0));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, tempTexture.handle));

    createCube();

    glCheck(glViewport(0, 0, CubemapSize, CubemapSize));
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, tempFBO.handle));
    for (auto i = 0u; i < 6u; ++i)
    {
        auto viewProj = projectionMatrix * viewMatrices[i];
        glCheck(glUniformMatrix4fv(uniforms.at("u_viewProjectionMatrix"), 1, GL_FALSE, &viewProj[0][0]));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_skyboxTexture, 0));

        glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        CRO_ASSERT(m_cubeVAO&& m_cubeVBO, "cube not correctly built!");
        //can't use VAOs on mobile - but this should be marked mobile incompatible anyway.
        glBindVertexArray(m_cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);


    deleteCube();
    return true;

#endif //PLATFORM_MOBILE
}

void EnvironmentMap::createCube()
{
    //position, normal, UV0

    std::vector<float> verts = {
        //back
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,         
         1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
        //front
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
         1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
        //left
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
        //right
         1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
         1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,         
         1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,     
        //bottom
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
        //top
        -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,     
         1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f        
    };

    glGenBuffers(1, &m_cubeVBO);
    glGenVertexArrays(1, &m_cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    //this kinda makes some assumptions about the attribute positions
    //so we'll have to be extra careful when writing the shaders
    glBindVertexArray(m_cubeVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
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