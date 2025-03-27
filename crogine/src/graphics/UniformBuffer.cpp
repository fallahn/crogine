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

    //we need to ensure that a UBO for a specific data block is always
    //bound to the same point, and that multiple UBOs for the same block
    //are NEVER bound at the same time.
    std::vector<std::uint32_t> activeBindings;
    std::vector<std::uint32_t> freeBindings;
    std::size_t freeBindingIndex = 0;

    //tracking the number of active instances means that we can
    //reset any assignments when the count reaches zero again and
    //we start afresh with the next batch of UBOs
    std::int32_t instanceCount = 0;
    std::unordered_map<std::type_index, std::uint32_t> bindPointIDs;
}
std::int32_t Detail::UniformBufferImpl::getMaxBindings() { return maxBindings; }

//-----class proper-----//
Detail::UniformBufferImpl::UniformBufferImpl(const std::string& blockName, std::size_t dataSize)
    : m_blockName           (blockName),
    m_bufferSize            (dataSize),
    m_ubo                   (0),
    m_bindPoint             (GL_INVALID_INDEX),
    m_instanceCountOffset   (1)
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

    instanceCount += m_instanceCountOffset;

//#ifdef CRO_DEBUG_
//    registerWindow([&,blockName]() 
//        {
//            if (ImGui::Begin(blockName.c_str()))
//            {
//                ImGui::Text("Available Binding Points: %d", maxBindings);
//                ImGui::Text("Shader Counts:");
//                if (m_refCount.empty())
//                {
//                    ImGui::Text("No Shaders active");
//                }
//                else
//                {
//                    for (const auto [handle, count] : m_refCount)
//                    {
//                        ImGui::Text("    ShaderID: %u, Count: %u", handle, count);
//                    }
//                }
//                ImGui::NewLine();
//                ImGui::Text("Active Binding Points:");
//                for (auto i = 0u; i < activeBindings.size(); ++i)
//                {
//                    if (activeBindings[i] != GL_INVALID_INDEX)
//                    {
//                        ImGui::Text("    Binding %u: UBO: %u", i, activeBindings[i]);
//                    }
//                }
//
//                ImGui::Text("%d UBOs are currently active", instanceCount);
//                ImGui::Separator();
//
//                ImGui::Text("Bind Point Mappings");
//                for (const auto& [t, bp] : bindPointIDs)
//                {
//                    ImGui::Text("%d, %u", t, bp);
//                }
//                ImGui::Separator();
//            }
//            ImGui::End();
//        });
//#endif

#else
    CRO_ASSERT(true, "Not available on mobile");
#endif

}

Detail::UniformBufferImpl::~UniformBufferImpl()
{
#ifdef PLATFORM_DESKTOP
    reset();

    instanceCount -= m_instanceCountOffset;

    if (instanceCount == 0)
    {
        //LogI << "Resetting assigned bind points" << std::endl;
        bindPointIDs.clear();
        freeBindingIndex = 0;
    }
#endif
}

Detail::UniformBufferImpl::UniformBufferImpl(Detail::UniformBufferImpl&& other) noexcept
    : m_blockName           (other.m_blockName),
    m_bufferSize            (other.m_bufferSize),
    m_ubo                   (other.m_ubo),
    m_bindPoint             (other.m_bindPoint),
    m_instanceCountOffset   (1)
{
    m_shaders.swap(other.m_shaders);

    other.m_blockName.clear();
    other.m_bufferSize = 0;
    other.m_ubo = 0;
    other.m_bindPoint = GL_INVALID_INDEX;
    other.m_instanceCountOffset = 0;

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
        m_instanceCountOffset = 1; //should already be constructed as so, but no harm in being specific

        m_shaders.swap(other.m_shaders);

        other.m_blockName.clear();
        other.m_bufferSize = 0;
        other.m_ubo = 0;
        other.m_bindPoint = GL_INVALID_INDEX;
        other.m_instanceCountOffset = 0;

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

        //force rebind the shaders on next bind
        unbind();
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

        //force rebind the shaders on next bind
        unbind();
    }
}

void Detail::UniformBufferImpl::bind()
{
#ifdef PLATFORM_DESKTOP
    if (activeBindings[m_bindPoint] != m_ubo)
    {
#ifdef CRO_DEBUG_
        CRO_ASSERT(m_bindPoint < maxBindings, "");
#endif

        //bind ubo to bind point
        glCheck(glBindBufferBase(GL_UNIFORM_BUFFER, m_bindPoint, m_ubo));
        activeBindings[m_bindPoint] = m_ubo;

        for (auto [shader, blockID] : m_shaders)
        {
            //bind to bind point
            glCheck(glUniformBlockBinding(shader, blockID, m_bindPoint));
        }
    }
#endif
}

void Detail::UniformBufferImpl::unbind()
{
    if (m_bindPoint < maxBindings)
    {
        if (activeBindings[m_bindPoint] == m_ubo)
        {
            activeBindings[m_bindPoint] = GL_INVALID_INDEX;
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

void Detail::UniformBufferImpl::setBindingPoint(std::type_index idx)
{
    if (bindPointIDs.count(idx) == 0)
    {
        bindPointIDs.insert(std::make_pair(idx, freeBindings[freeBindingIndex++]));
    }

    m_bindPoint = bindPointIDs.at(idx);
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