/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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

#include "../detail/GLCheck.hpp"
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/Shader.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/Gui.hpp>
#endif


using namespace cro;

namespace
{
    std::int32_t maxBindings = -1;
}

std::int32_t Detail::UniformBufferImpl::getMaxBindings() { return maxBindings; }
std::vector<std::uint32_t> Detail::UniformBufferImpl::m_activeBindings;

Detail::UniformBufferImpl::UniformBufferImpl(const std::string& blockName, std::size_t dataSize)
    : m_blockName   (blockName),
    m_bufferSize    (dataSize),
    m_ubo           (0)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(!blockName.empty(), "");
    CRO_ASSERT(dataSize, "");

    //create buffer
    glCheck(glGenBuffers(1, &m_ubo));
    glCheck(glBindBuffer(GL_UNIFORM_BUFFER, m_ubo));
    glCheck(glBufferData(GL_UNIFORM_BUFFER, dataSize, NULL, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    if (maxBindings == -1)
    {
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxBindings);
        m_activeBindings.resize(maxBindings);
        std::fill(m_activeBindings.begin(), m_activeBindings.end(), GL_INVALID_INDEX);
    }

#ifdef CRO_DEBUG_
    registerWindow([&,blockName]() 
        {
            if (ImGui::Begin(blockName.c_str()))
            {
                ImGui::Text("Available Binding Points: %d", maxBindings);
                ImGui::Text("Shader Counts:");
                if (m_refCount.empty())
                {
                    ImGui::Text("No Shaders active");
                }
                else
                {
                    for (const auto [handle, count] : m_refCount)
                    {
                        ImGui::Text("    ShaderID: %u, Count: %u", handle, count);
                    }
                }
                ImGui::NewLine();
                ImGui::Text("Active Binding Points:");
                for (auto i = 0u; i < m_activeBindings.size(); ++i)
                {
                    if (m_activeBindings[i] != GL_INVALID_INDEX)
                    {
                        ImGui::Text("    Binding %u: UBO: %u", i, m_activeBindings[i]);
                    }
                }
            }
            ImGui::End();
        });
#endif

#else
    CRO_ASSERT(true, "Not available on mobile");
#endif

}

Detail::UniformBufferImpl::~UniformBufferImpl()
{
#ifdef PLATFORM_DESKTOP
    reset();
#endif
}

Detail::UniformBufferImpl::UniformBufferImpl(Detail::UniformBufferImpl&& other) noexcept
    : m_blockName   (other.m_blockName),
    m_bufferSize    (other.m_bufferSize),
    m_ubo           (other.m_ubo)
{
    m_shaders.swap(other.m_shaders);

    other.m_blockName.clear();
    other.m_bufferSize = 0;
    other.m_ubo = 0;

    other.m_shaders.clear();
}

const Detail::UniformBufferImpl& Detail::UniformBufferImpl::operator = (Detail::UniformBufferImpl&& other) noexcept
{
    if (&other != this)
    {
        reset();

        m_blockName = other.m_blockName;
        m_bufferSize = other.m_bufferSize;
        m_ubo = other.m_ubo;

        m_shaders.swap(other.m_shaders);

        other.m_blockName.clear();
        other.m_bufferSize = 0;
        other.m_ubo = 0;

        other.m_shaders.clear();
    }
    return *this;
}

//public
void Detail::UniformBufferImpl::addShader(const Shader& shader)
{
    addShader(shader.getGLHandle());
}

void Detail::UniformBufferImpl::addShader(std::uint32_t handle)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(handle, "");

    //if shader has uniform block matching our name, add
    const auto blockIndex = glGetUniformBlockIndex(handle, m_blockName.c_str());
    if (blockIndex != GL_INVALID_INDEX)
    {
        m_shaders.emplace_back(handle, blockIndex);

        if (m_refCount.count(handle) == 0)
        {
            m_refCount.insert(std::make_pair(handle, 1));
        }
        else
        {
            m_refCount.at(handle)++;
        }
    }
#ifdef CRO_DEBUG_
    else
    {
        LogW << "Shader " << handle << " was not added to " << m_blockName << std::endl;
    }
#endif
#endif
}

void Detail::UniformBufferImpl::removeShader(const Shader& shader)
{
    removeShader(shader.getGLHandle());
}

void Detail::UniformBufferImpl::removeShader(std::uint32_t handle)
{
    if (m_refCount.count(handle)
        && m_refCount.at(handle) > 0)
    {
        m_refCount.at(handle)--;
        m_shaders.erase(std::remove_if(m_shaders.begin(), m_shaders.end(), 
            [handle](const std::pair<std::uint32_t, std::uint32_t>& p)
            {
                return p.first == handle;
            }),
            m_shaders.end());
    }
}

void Detail::UniformBufferImpl::bind(std::uint32_t bindPoint)
{
#ifdef PLATFORM_DESKTOP
    if (m_activeBindings[bindPoint] != m_ubo)
    {
#ifdef CRO_DEBUG_
        /*std::int32_t maxBindings = 0;
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxBindings);*/
        CRO_ASSERT(bindPoint < maxBindings, "");
#endif
        //bind ubo to bind point
        glCheck(glBindBufferBase(GL_UNIFORM_BUFFER, bindPoint, m_ubo));
        m_activeBindings[bindPoint] = m_ubo;

        for (auto [shader, blockID] : m_shaders)
        {
            //bind to bind point
            glCheck(glUniformBlockBinding(shader, blockID, bindPoint));
        }
    }
#endif
}

//protected
void Detail::UniformBufferImpl::setData(const void* data)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(data, "");
    CRO_ASSERT(m_bufferSize, "");
    CRO_ASSERT(m_ubo, "");

    glCheck(glBindBuffer(GL_UNIFORM_BUFFER, m_ubo));
    glCheck(glBufferData(GL_UNIFORM_BUFFER, m_bufferSize, data, GL_DYNAMIC_DRAW));

#endif // PLATFORM_DESKTOP
}

//private
void Detail::UniformBufferImpl::reset()
{
#ifdef PLATFORM_DESKTOP
    if (m_ubo)
    {
        glCheck(glDeleteBuffers(1, &m_ubo));
        m_ubo = 0;
    }
#endif
}