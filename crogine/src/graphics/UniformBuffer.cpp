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

#include <numeric>

using namespace cro;

namespace
{
    std::int32_t maxBindings = -1;


    std::vector<std::uint32_t> activeBindings;
    std::vector<std::uint32_t> freeBindings;
    std::size_t freeBindingIndex = 0;

    //used to manage the next free available binding point
    std::uint32_t getBindingIndex()
    {
        CRO_ASSERT(freeBindingIndex < maxBindings, "");
        return freeBindings[freeBindingIndex++];
    }

    void releaseBindingIndex(std::uint32_t bp)
    {
        if (auto res = std::find(freeBindings.begin(), freeBindings.begin() + freeBindingIndex, bp);
            res != freeBindings.begin() + freeBindingIndex)
        {
            freeBindingIndex--;
            *res = freeBindings[freeBindingIndex];
            freeBindings[freeBindingIndex] = bp;

            LogI << "Freed binding point " << bp << std::endl;
        }
    }
}
std::int32_t Detail::UniformBufferImpl::getMaxBindings() { return maxBindings; }

//-----class proper-----//
Detail::UniformBufferImpl::UniformBufferImpl(const std::string& blockName, std::size_t dataSize)
    : m_blockName   (blockName),
    m_bufferSize    (dataSize),
    m_ubo           (0),
    m_bindPoint     (GL_INVALID_INDEX)
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
        activeBindings.resize(maxBindings);
        std::fill(activeBindings.begin(), activeBindings.end(), GL_INVALID_INDEX);

        freeBindings.resize(maxBindings);
        std::iota(freeBindings.begin(), freeBindings.end(), 0);
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
                for (auto i = 0u; i < activeBindings.size(); ++i)
                {
                    if (activeBindings[i] != GL_INVALID_INDEX)
                    {
                        ImGui::Text("    Binding %u: UBO: %u", i, activeBindings[i]);
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
    m_ubo           (other.m_ubo),
    m_bindPoint     (other.m_bindPoint)
{
    m_shaders.swap(other.m_shaders);

    other.m_blockName.clear();
    other.m_bufferSize = 0;
    other.m_ubo = 0;
    other.m_bindPoint = GL_INVALID_INDEX;

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
        m_bindPoint = other.m_bindPoint;

        m_shaders.swap(other.m_shaders);

        other.m_blockName.clear();
        other.m_bufferSize = 0;
        other.m_ubo = 0;
        other.m_bindPoint = GL_INVALID_INDEX;

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

void Detail::UniformBufferImpl::bind()
{
#ifdef PLATFORM_DESKTOP
    if (m_bindPoint == GL_INVALID_INDEX ||
        activeBindings[m_bindPoint] != m_ubo)
    {
        //hmmm we could check if we're bound elsewhere
        //but the index management *should* assure we
        //never have that happen

        const auto bindPoint = getBindingIndex();

#ifdef CRO_DEBUG_
        CRO_ASSERT(bindPoint < maxBindings, "");
#endif

        //bind ubo to bind point
        glCheck(glBindBufferBase(GL_UNIFORM_BUFFER, bindPoint, m_ubo));
        activeBindings[bindPoint] = m_ubo;

        for (auto [shader, blockID] : m_shaders)
        {
            //bind to bind point
            glCheck(glUniformBlockBinding(shader, blockID, bindPoint));
        }
        m_bindPoint = bindPoint;
    }
#endif
}

void Detail::UniformBufferImpl::unbind()
{
    if (m_bindPoint < maxBindings)
    {
        if (activeBindings[m_bindPoint] == m_ubo)
        {
            releaseBindingIndex(m_bindPoint);
            activeBindings[m_bindPoint] = GL_INVALID_INDEX;
            m_bindPoint = GL_INVALID_INDEX;
        }
    }
}

//protected
void Detail::UniformBufferImpl::setData(const void* data)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(data, "");
    CRO_ASSERT(m_bufferSize, "");
    CRO_ASSERT(m_ubo, "");

    glCheck(glBindBuffer(GL_UNIFORM_BUFFER, m_ubo));
    glCheck(glBufferSubData(GL_UNIFORM_BUFFER, 0, m_bufferSize, data));

#endif // PLATFORM_DESKTOP
}

//private
void Detail::UniformBufferImpl::reset()
{
#ifdef PLATFORM_DESKTOP
    if (m_ubo)
    {
        unbind();

        glCheck(glDeleteBuffers(1, &m_ubo));
        m_ubo = 0;
    }
#endif
}